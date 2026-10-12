# Industrial tower: expert parkour blockout

Scene: `assets/models/akaza animations/akaza player2.json`.

## Finding the course

The uniformly doubled tower is detached from the arena, centered at `(80, 0, 16)`. Its entry is `(66, 0, 2)`, summit Y=48 and highest beam Y=52.6. Its foundation spans X=56..104 and Z=-7..39, east of the arena wall. Frame `Tower_Entry` and place the player near `(66, 0.05, 2)` in the editor for testing; no connecting bridge or teleport has been added. Existing arena entities and movement code are unchanged.

The previous course has been completely replaced. New entities start with `Tower_` and retain the `ParkourShowcase` tag. Frame `Tower_Entry` or `Tower_Crown` in the editor. Reload the scene from disk rather than saving the old in-memory layout over it.

## Routes

The route descriptions below retain **original design-space coordinates and dimensions** for reference. Convert them to the current scene with `x' = 80 + 2*(x-28)`, `y' = 2*y`, `z' = 16 + 2*(z-16)`. All lengths and mover speeds are doubled; waits and cycle durations stay unchanged. Thus gaps are now 6m, core ledge rises 3m, vault barriers 2m, and service decks are at Y=12/24/36/48. With unchanged player abilities, some original routes are no longer reachable; this is intentional uniform scaling, not a traversal rebalance.

### Exterior: wall-run, kick-off, moving transfer

Tier 1 starts at `(21, 1.2, 11)`. Build speed along the cyan-marked wall at X=19.75, jump/hold WallRun across the 3m break, and catch the 1.6m-wide pad at `(21, 2.4, 16)`. Jump or kick off toward the amber pad at `(23.2, 3.6, 19)`.

Wait for the 2m shuttle at Z=21.5, board it, ride across to X=31, then jump to the higher transfer at `(33, 4.8, 21.5)`. A final diagonal jump reaches the 3m-square service deck at `(35, 6, 23)`. This is the starting end of Tier 2, rotated 180 degrees. Repeat the chain to Y=12, Y=18 and Y=24, alternating tower faces. At the final deck, use the crown link pads near `(22, 24, 10)` and `(24, 24, 12)` to reach the crown.

No wide staircase bypass remains. Launch/catch pads are 1.6–1.8m wide, rise 1.2m per transfer and require deliberate speed control. The shuttles have 7.8m travel, speeds of 1.8/1.95/2.1/2.25m/s and 0.75s endpoint waits. Missing a transfer can drop you several tiers.

### Interior: maintenance gate into jump shaft

From entry, turn onto the 2.2m maintenance lane at X=24.5. Sprint-vault its 1m barrier at Z=10, then time a slide or dodge through the red shutter at Z=13.5 while it is raised. Warning bands mark the crossing. Continue through the core entrance toward `(28, 0, 16)`.

The shaft has 4.5m clear width and sixteen staggered 1.3m × 1.25m ledges, rising 1.5m at a time. Alternate wall jumps, adjust longitudinal position between Z=15.2 and 16.8 and avoid jumping directly into the ledge overhead. This is the precision route, not a broad recovery staircase. At service levels, the small moving core bridges offer transfers toward the exterior decks. Higher maintenance lanes are alternate entries from the service decks.

### Timed hazards and readability

Four solid red shutters travel 3.2m vertically at 1.6m/s, waiting 0.9s at each endpoint: a 5.8s complete cycle. At the raised endpoint there is 3.2m clearance. Wait behind the warning band and cross during the open phase; do not treat the descending shutter as a platform. The housing above indicates the retraction direction. All gates begin open; they are visible mechanical obstacles rather than invisible damage volumes.

Steel columns, ring beams, machinery banks, ducts and louvers establish an industrial silhouette. Cyan identifies traversal surfaces and moving platforms; amber identifies transfer/deck targets; red identifies moving obstructions. These are blockout materials and primitive-built industrial details, not final production art.

## Rest and recovery

Only four 3m-square service decks at Y=6/12/18/24 interrupt the exterior climb. The interior ledges are small catch opportunities, not comfortable staging floors. The courtyard remains below; a fall can require restarting from the entrance.

There are **no saved respawn checkpoints or newly implemented damage rules**. Shutters are real moving colliders and can block/displace the player; they are not scripted instant-kill traps. Elevated edges remain open. This change intentionally leaves the working movement system untouched.

## Default controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Jump / wall-jump | Space | A |
| Wall-run (hold) | R | Right trigger |
| Sprint | Left Shift | Left stick click |
| Slide while sprinting / crouch | Left Ctrl | B |
| Vault | V | Left trigger |
| Dodge | Q | Right bumper |

Custom input profiles override these defaults. Sprint before sliding or vaulting.

## Geometry notes and validation

- Cubes use local collider half-extents `(0.5, 0.5, 0.5)` so visible and colliding dimensions agree.
- Current wall-run gaps are 6m, exceeding nominal 3.75m run travel. Movement settings were not increased to compensate.
- Slides still use the standing capsule. Cross the timed gate while fully open; this is a speed/timing challenge, not a slide-only low ceiling.
- `python tools/build_parkour_tower.py` replaces only tagged course entities and preserves other entities. `--check` validates without writing. Generation is deterministic.
- `StartupArenaSmokeTest` checks tower counts, collision dimensions, moving endpoints/speeds/waits, gap lengths, ledge heights and summit geometry. Static tests do not simulate the mover loop in main.cpp or certify route completion.
- In-game acceptance pass: complete each exterior tier in both directions, board/dismount each shuttle and core bridge, climb the full core, test gate collisions at multiple frame rates, and verify no housing/beam obscures jump sightlines. Expert balance and production-level presentation still require that playtest/art pass.

## Performance guidance (not measured)

The tower uses 154 cube entities plus its new foundation, with 12 movers. The cubes are already low polygon; per-entity draw submissions, shadow passes and collision checks may be more important than triangles. No profiling samples were available, so this change makes no FPS improvement claim.

Recommended next steps are to batch static decorative geometry by material and spatial section, keep movers independent, remove collision participation from purely decorative details where safe, and limit decorative shadow casting if supported. Preserve traversal colliders and use section-level visibility/LOD rather than one giant always-visible mesh. Measure frame times before/after each change; uniform scaling alone does not reduce rendering or physics cost.
