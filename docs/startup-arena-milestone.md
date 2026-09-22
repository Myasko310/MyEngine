# Startup arena — milestone 1 review gate

## Scope

Expand the actual saved startup scene without replacing the player's scene at boot. This milestone is a playable arena foundation, not sign-off on the complete performance/visual test matrix or the 120 FPS target.

- Runtime selection: `build/debug/Debug/startup_scene.txt` points to `assets/models/akaza animations/akaza player2.json`.
- Visual Studio's `.vs/launch.vs.json` uses `build/debug/Debug` as the working directory. No new root startup configuration is introduced.
- All 12 original entities (including the player asset, animations, transform, controller, camera and light) are unchanged. Layer names, collision matrix and global scripts are unchanged. Verified structurally against the previously clean scene in Git.
- The scene now contains 60 entities: 12 original plus 48 additions tagged `ArenaExpansion`. No generation occurs on launch, so repeated launches cannot duplicate the arena or overwrite editor adjustments.
- Unrelated boss-battle changes are left intact.

## Layout

The courtyard spans 64 by 64 world units, centered at `(0, 0, -18)`, with a wide central fighting space and four cover pieces. Two open-front stone halls have accessible roofs; gate piers/lintel, watchtowers, cornices, parapets and colored banners frame the space. Existing dungeon stone textures are reused; there are no added lights or imported high-poly buildings.

Three color-coded routes retain the saved 4.5 movement speed and 6.0 jump speed:

| Route | Entry | Destination | Character |
|---|---|---|---|
| Teal / west | `(-20, 0, -5.3)` | West hall roof, height 4 | Straight ascending platforms |
| Rust / east | `(20, 0, -5.3)` | East hall roof, height 4 | Alternating lateral offsets |
| Gold / north | `(-8.7, 0, -37)` | Elevated podium, height 4 | Cross-courtyard ascending route |

Each has five platforms and a sixth destination. Falls return to the original ground plane/courtyard; this milestone adds no death/reset mechanic or new combat AI.

## Correctness changes

1. `main.cpp` loads the saved scene without erasing entities, teleporting the player, or substituting a camera. The player pointer is rebound to the loaded entity and stale bootstrap cube references are cleared on successful load.
2. New box colliders use local half extents `(0.5, 0.5, 0.5)`; the transform supplies world scaling once. The discarded launch injection scaled both, producing oversized collision volumes.
3. Solid arena pieces retain kinematic rigidbodies because the existing dynamic collision pass requires them. Decorative banner bounds are explicitly triggers to prevent automatically generated primitive spheres from becoming invisible solid obstacles.
4. The new traversal test reproduced a second bug on `Arena_West_02`: snap leaves a skin-width gap, but the next support query used only actual overlap, discarding a short jump press. `PhysicsSystem` now rechecks that small gap for previously grounded, non-rising characters before processing jump input. No controller parameters changed.
5. Shader auto-reload checks are throttled to one per 250 ms per shader, including failed compiles. Explicit `ReloadFromDisk` remains immediate.

## Measurement record

Both CPU captures use the active startup application, CPU sampling at 1,000 samples/second.

- Before: session `9e3632f5-ddf3-409d-9f91-30eeb0eb34c0`; 271,515 process samples; 346.896-second capture.
- After: session `be9e7d98-c057-44c0-ba7c-1d1a6cb0573f`; 238,099 process samples; 245.224-second capture.

| Function | Before self % | After self % | Before total % | After total % | Total-share delta |
|---|---:|---:|---:|---:|---:|
| `Shader::ShouldHotReload` (all callers) | 0.0188 | 0.0004 | 37.6804 | 0.6094 | -37.0710 percentage points |
| `MeshRendererSystem::Render` | 0.1300 | 0.1147 | 48.3134 | 22.9384 | -25.3750 percentage points |

Hot-reload inclusive samples: 102,308 before, 1,451 after. Renderer inclusive samples: 131,178 before, 54,616 after. The baseline hot lines were the vertex/fragment timestamp queries in `Shader.cpp` (51,239 and 50,703 samples respectively).

**These are CPU distributions, not FPS improvements or an equivalent-workload benchmark.** Capture lengths, scene content and interactive workload differ. The before capture did not contain `PhysicsSystem::OnUpdate`; the after capture did (13.26% total). Do not infer a physics regression from absent baseline samples. After-capture runtime hotspots also include animation (17.19%) and the editor scene hierarchy (14.46%). No further optimization of those paths is claimed.

GPU frame timing exists in the engine's performance panel, but these CPU captures do not supply GPU measurements. GPU bottlenecks, frame pacing and 120 FPS have not been validated.

## Automated validation gate

- Full workspace build passed.
- CMake smoke suite passed after the reproduced jump failure was fixed.
- `StartupArenaSmokeTest` reads the actual scene and uses the real serializer and physics system. It retains saved physics settings but removes graphics/animation/script components for headless physics testing.
- Tests check unique IDs and local collider extents, spawn jump/apex/landing, continuous traversal of all six landings in each route, and a dynamic sphere landing on the elevated podium. Runs use 30, 60 and 144 Hz input with the engine's unchanged 50 Hz physics timestep. Only route entry placement is reset; successive hops are not teleported.
- `ShaderPollingSmokeTest` uses a hidden real OpenGL context: unchanged-source polling, throttling, eventual changed-source reload, failed-compile program retention, and immediate manual reload.
- Existing serializer, Lua, input, animation, rendering, networking, plugin and terrain smoke cases remain unchanged.

Re-run with `ctest --test-dir build/debug -C Debug --output-on-failure` after building. Logs are in `build/debug/Testing/Temporary/LastTest.log`.

## Remaining review gates

1. Review the architecture and route difficulty in Play mode with the saved camera and actual keyboard/gamepad bindings. Physics input simulation is not a visual/gameplay feel assessment.
2. Capture matched Release gameplay runs on the same route and camera path, recording CPU frame time, GPU time, FPS and frame-time percentiles. Compare identical workloads before claiming frame-rate gains.
3. Complete the broader automated performance matrix (including movement/camera/world-position smoothness and supported hardware) before declaring the arena feature complete. Use a separate milestone/PR for further engine optimization or visual expansion.
