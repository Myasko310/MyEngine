# Startup arena — milestone 2: bounded broadphase insertion

## Scope and review gate

Reduce the measured physics cost of the existing expanded startup arena. No scene entities, player settings, assets, camera behavior, animation data, rigidbodies or jump logic are changed in this milestone. This is not sign-off on the 120 FPS or GPU/frame-pacing targets.

The startup asset SHA-256 before this milestone was `E1FCF54FD161B4DADDCB6A7231B4B9F1DE7541672C46BDB2BC3B3D9F9BEC4C19` (`assets/models/akaza animations/akaza player2.json`). Unrelated working changes are retained.

## Measured cause

The existing post-milestone-1 application capture, session `be9e7d98-c057-44c0-ba7c-1d1a6cb0573f`, contained 31,583 samples under `PhysicsSystem::OnUpdate`. Of these, 11,443 were under `BroadphaseGrid::InsertAABB` and 6,733 under `Clear`.

The hottest insertion line was `cells[HashCell(x, y, z)].push_back(entity)` (11,427 inclusive samples). Large arena floors and walls were replicated across many cells and then discarded every fixed step. The 64-by-64 courtyard floor alone spans 2,178 cells at the default two-unit cell size.

## Change

- A collider covering at most 64 cells follows the existing spatial-grid path.
- Larger colliders are recorded once and checked using inclusive AABB overlap against small colliders and other oversized colliders.
- Shared callback deduplication retains unique, canonically ordered entity pairs.
- Bounds are rebuilt every fixed step. Moving/resized geometry cannot leave stale persistent bounds; `Clear` releases entity references from both paths.
- Ordinary dynamic bodies and kinematic arena geometry still enter collision detection. No rigidbodies are removed and no collision layers, narrow-phase responses or character support logic are changed.

The fallback performs O(L*S + L^2) AABB checks for L oversized and S small entries. It is intended for arenas with relatively few large floors/walls; scenes dominated by oversized colliders may need a hierarchical broadphase in a separate measured milestone. The threshold is an initial bounded policy, not a claim of universal optimality.

## Validation

Full workspace build passed. `ctest --test-dir build/debug -C Debug --output-on-failure` passed the complete smoke suite (19.01 seconds in this run).

- Existing `StartupArenaSmokeTest` and `ShaderPollingSmokeTest` are unchanged.
- All three routes still traverse all six landings at 30, 60 and 144 Hz input, including spawn jumping/landing and dynamic-body collision checks.
- New `ArenaBroadphaseSmokeTest` validates every brute-force AABB overlap is present in the candidate set, allowing harmless extra broadphase candidates.
- Coverage includes mixed collider sizes, large-large and large-small overlaps, exact boundaries, negative coordinates, randomized fixtures, different cell sizes, threshold crossings, movement/resizing across rebuilds, reversed insertion order, duplicate insertion, bounded grid references, and releasing removed entities.

## Re-measurement

The same active-startup-app CPU sampling collection was used after the change: session `fc9a0511-6b0a-40ac-84c3-2ba60f59b6f2`.

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| Capture duration | 245.224 s | 63.804 s | -181.420 s |
| Process CPU samples | 238,099 | 49,854 | -188,245 |
| `OnUpdate` inclusive samples | 31,583 | 1,152 | -30,431 |
| `DetectAndResolveCollisions` inclusive samples | 21,907 | 369 | -21,538 |
| `InsertAABB` inclusive samples | 11,443 | 108 | -11,335 |
| `Clear` inclusive samples | 6,733 | 45 | -6,688 |
| Collision detection share of physics samples | 69.36% | 32.03% | -37.33 pp |
| Insertion + clearing share of physics samples | 57.55% | 13.28% | -44.27 pp |

"Share of physics samples" uses `OnUpdate` inclusive samples as the denominator to avoid directly comparing startup-heavy whole-process percentages. Absolute sample-count deltas are recorded for transparency, **not** interpreted as speedups.

| Function | Before self % | After self % | Before total % | After total % |
|---|---:|---:|---:|---:|
| `OnUpdate` | 0.0004 | 0 | 13.2647 | 2.3107 |
| `DetectAndResolveCollisions` | 0.0042 | 0.0020 | 9.2008 | 0.7402 |
| `InsertAABB` | 0.0185 | 0.0060 | 4.8060 | 0.2166 |
| `Clear` | 0 | 0 | 2.8278 | 0.0903 |

These latter percentages use all process samples. Both captures contain active physics, but their duration and interactive workloads differ. The new shorter capture is dominated by scene/animation import. This supports reduced grid-maintenance share; it does not establish matched per-frame CPU time, an FPS improvement, or GPU performance. The larger relative share of character-controller work after this change is not itself proof of a regression.

## Next review gates

1. Review in-game movement and contacts with the preserved scene, including other moving/large geometry.
2. Establish matched Release gameplay runs, fixed camera/input paths, CPU/GPU frame times and frame-time percentiles before claiming FPS gains.
3. If further optimization is requested, profile character support queries or the editor hierarchy independently; keep scene content and existing regression tests fixed.
