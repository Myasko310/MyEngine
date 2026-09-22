# Startup arena — milestone 4: visibility correctness and hierarchy cost

## Scope

Fix the reported scenery popping and the measured editor hierarchy CPU hotspot. Scene content, player settings, cameras, materials, lights, physics and the previous arena optimizations are preserved.

Startup scene SHA-256 before this milestone: `D39DE73E6C95CEF0DE7849589DA2C1D89F937A4A18C86A43627E7A884777AD62`.

## Popping: confirmed culling defect

The main camera pass tested `tc.position + bs.center` and the unscaled local radius against a world-space frustum. A large floor or wall could therefore be rejected when its small local sphere left the camera view, despite visible geometry remaining on screen. Parent transforms and transformed local centers were ignored as well.

The main pass now resolves the world matrix and transforms the bounding sphere before both frustum and projected-size tests. `RenderBounds.h` bounds the maximum linear stretch with the square root of the largest absolute row sum of A-transpose times A. This is conservative under rotation, negative/nonuniform scale and shear introduced by parent transforms; a maximum-column-length shortcut is not sufficient for general shear.

Culling remains enabled. The optional GPU occlusion-query implementation, LOD policy, animation bounds and shadow-pass implementation were not redesigned. This fix addresses the reproduced camera-bounds defect, not a claim that all possible sources of visual popping have been exhausted.

## CPU: hierarchy indexing

Baseline session `39669b8b-5119-47c2-805f-2e016e0f2c0f` attributed 58,155 samples (29.56% of process samples) to `DrawSceneHierarchyPanel`. The call to `drawEntityNode` had 57,565 inclusive samples. The node routine repeatedly scanned every entity to find children and traversed the scene again for each expanded node, producing quadratic work for a flat arena.

`SceneHierarchyIndex` now builds ordered roots and parent-to-child lists once per visible panel frame. Tree drawing visits those lists instead of repeatedly querying every scene entity. The index is transient, so reparenting and new/deleted entities do not need a persistent cache invalidation mechanism. A collapsed panel exits after pairing ImGui Begin/End. Deletion is deferred until tree traversal finishes, retaining the existing child-unparenting and undo snapshot behavior without modifying containers being traversed.

Root order, existing filtering behavior, selection and drag/drop handling are retained. This milestone does not change filter semantics for nested matches.

## Automated validation

- Full workspace build passed.
- Complete CMake smoke suite passed in 34.75 seconds in the recorded run.
- Existing jumping-route tests still passed at 30, 60 and 144 Hz input; district walking access, dynamic collision, shader and broadphase tests also passed unchanged.
- New `SceneVisibilitySmokeTest` exercised 1,556 visible-corner cases from the saved arena across 24 camera orientations. Of these, 295 would be rejected by the old unscaled sphere test; the corrected bounds retained all visible-corner cases.
- The test also covers a deliberately off-screen-center/scaled object, rejection of a genuinely distant sphere, transformed centers, negative/uniform/nonuniform scale, and sampled surface points under parent-induced shear.
- New `SceneHierarchyIndexSmokeTest` covers ordered roots, nested children, entities without transforms, null entries, self-parent exclusion, reparenting, unparenting, deletion/rebuilding, 200 extra flat roots, and stable snapshots.

These are geometry/physics/data-structure regressions, not rendered-image comparisons or automated mouse-driven editor/undo tests. Visual camera sweeps and interactive hierarchy editing remain part of the review gate.

Re-run with `ctest --test-dir build/debug -C Debug --output-on-failure` after building.

## CPU re-measurement

Both captures use active-startup-application CPU sampling at 1,000 samples/second with the same saved scene.

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| Session | `39669b8b-5119-47c2-805f-2e016e0f2c0f` | `22ab6900-cb19-4aad-bb99-9a6c15612bd8` | — |
| Capture duration | 201.710 s | 302.470 s | +100.760 s |
| Process CPU samples | 196,722 | 298,661 | +101,939 |
| Hierarchy inclusive samples | 58,155 | 6,386 | -51,769 |
| Hierarchy self samples | 9 | 77 | +68 |
| Hierarchy total CPU share | 29.562% | 2.138% | -27.424 pp |
| Hierarchy self CPU share | 0.0046% | 0.0258% | +0.0212 pp |
| Render command execution total share | 22.294% | 58.601% | +36.307 pp |

Self/inclusive percentages are not milliseconds per frame. The small hierarchy self-sample increase does not cancel the reduction in its inclusive work. Capture lengths, camera activity and interactive workloads differ; absolute sample counts and process shares must not be interpreted as FPS improvements or matched-workload regressions.

The corrected renderer may process objects that were previously incorrectly culled. In the after capture `MeshRendererSystem::Render` accounts for 57.29% of CPU samples, including 18.13% under its `TransformHierarchy::GetWorldMatrix` calls. Rendering is now the leading measured branch. No FPS, GPU-time or 120 FPS claim is made.

## Review gate / next milestone

1. Sweep the camera around the saved arena and review formerly popping floors, walls, galleries and decorations. Verify drag/drop, selection, unparenting, deletion and undo interactively.
2. Profile repeated world-transform work for a separate renderer optimization milestone; avoid persistent transform caches without correct invalidation.
3. Use matched Release gameplay/camera runs with CPU/GPU frame timings and percentiles before claiming frame-rate gains. GPU timing remains unverified by these CPU captures.
