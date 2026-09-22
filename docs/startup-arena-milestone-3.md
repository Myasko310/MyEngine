# Startup arena — milestone 3: architectural districts

## Delivered scene content

Expanded the actual startup asset, `assets/models/akaza animations/akaza player2.json`, rather than injecting a replacement at launch.

| Scene measure | Previous | Expanded | Change |
|---|---:|---:|---:|
| Courtyard footprint | 64 x 64 | 96 x 96 | 1.5x each dimension |
| Courtyard area | 4,096 square units | 9,216 square units | 2.25x |
| Entity count | 60 | 113 | +53 |

Bounds are now approximately x = -48 to 48, z = -66 to 30. The arena remains centered at `(0, 0, -18)`.

New entities use the `ArenaDistrict` tag and IDs 200–259 with intentional gaps. They are ordinary editable scene content, not generated on startup.

- **North temple court:** raised terrace, shallow entrance stairs, four-column portico, roof/frieze, crest, shrine plinth/monolith and banners. Approach through the existing gate near `(0, 0, -46)` and continue north toward `(0, 0.9, -57)`.
- **West gallery:** covered colonnade, windows, cornice and a roof-level promenade. A bridge connects the original west jump-route roof to the gallery at height 4. The old west roof parapet was split to leave a three-unit passage aligned with the bridge.
- **East market:** taller covered colonnade, colored awnings, shutter panels and counters. Enter between the columns near `(32, 0, -14)`.
- **Outer scenery:** six perimeter buttresses, expanded south courtyard, and a southern monument.

The five existing courtyard/perimeter pieces were resized/repositioned, and the west parapet was shortened to form the bridge opening. The original 12 user-scene entities, player assets/animations/settings, camera, lighting, collision matrix and global scripts are unchanged. The existing three jump routes and their platform dimensions are unchanged.

## Budget and collision policy

The district reuses the existing cube mesh primitive and existing stone textures; no new imported model packages, texture files or lights were introduced. This controls asset proliferation but is not a claim of unchanged rendering cost: there are 53 additional mesh instances.

All new solid pieces use local box half extents `(0.5, 0.5, 0.5)` with kinematic rigidbodies. Thin decorative banners/window/shutter panels explicitly mark their bounding spheres as triggers so scene loading does not generate invisible solid obstacles. The previous bounded-broadphase optimization remains in place. No engine physics/controller or renderer production code changed in this milestone.

## Regression validation

Full workspace build passed. The complete CMake smoke suite passed in 34.78 seconds in the final run.

- Existing `StartupArenaSmokeTest` is unchanged and still verifies all three complete jumping routes at 30, 60 and 144 Hz input, spawn jumping/landing and dynamic-body collision.
- Existing broadphase, shader and other subsystem smoke cases remain unchanged.
- New `ArenaDistrictSmokeTest` reads the saved scene, checks district collider authoring and texture paths, then loads the saved physics via the real serializer without GPU/animation/script imports.
- At 60 Hz input and the unchanged 50 Hz physics timestep, it walks the temple stairs, both covered galleries, the rooftop bridge, and the south expansion. These checks do not jump, teleport between intermediate steps, or alter the saved controller parameters.
- The first access test exposed that 0.3-unit temple steps blocked the controller. The scene was corrected to five 0.15-unit rises followed by the terrace landing, and the same access assertions then passed. Controller settings and access-test tolerances were not relaxed.

Re-run using `ctest --test-dir build/debug -C Debug --output-on-failure` after building. Detailed results are in `build/debug/Testing/Temporary/LastTest.log`.

## Post-expansion CPU capture

Session `39669b8b-5119-47c2-805f-2e016e0f2c0f`, CPU sampling at 1,000 samples/second, capture duration 201.710 seconds, 196,722 process samples. The active startup application was used and the capture contains active physics.

| Application branch | Inclusive samples | Self samples | Total CPU share | Self CPU share |
|---|---:|---:|---:|---:|
| `DrawSceneHierarchyPanel` | 58,155 | 9 | 29.56% | 0.005% |
| Render command execution | 43,857 | 0 | 22.29% | 0.000% |
| `PhysicsSystem::OnUpdate` | 29,255 | 1 | 14.87% | 0.001% |
| `LoadScene` | 23,309 | 0 | 11.85% | 0.000% |
| `AnimationSystem::Update` | 21,798 | 11 | 11.08% | 0.006% |

The editor hierarchy is the largest measured application branch in this run. The prior capture was much shorter (63.804 seconds) and dominated by startup import; directly comparing whole-process shares would not establish a frame-time regression or gain. This is a content expansion, not a demonstrated speedup. GPU timing, matched Release frame-time percentiles and 120 FPS remain unverified.

## Review gate

Review the architecture, sightlines, route readability and camera experience in Play mode before adding further detail. Automated physics tests establish the tested access paths, not visual quality or gameplay feel. A separate measured optimization milestone can investigate editor hierarchy cost; keep the current scene and existing regression tests fixed for that comparison.
