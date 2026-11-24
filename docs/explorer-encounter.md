# Explorer encounter

Turn **Explorer: On** before Play. Use **Follow Me** to call the explorer or **Resume Exploring** to let it investigate the map. All behavior is noncombat.

Turn **Sparring: On** before single-player Play. The partner starts by spacing, then closes in to jab if you leave an opening. Swing nearby and it tries to dodge, then counter. Among attacks in reach it prefers the strongest whose startup fits your remaining recovery with two frames to spare. For short openings it tries the quickest reachable attack. Both opening attacks and counters use normal melee contact and can miss.

Move behind a wall with a walkable way around it to try pursuit. The partner uses the navigation grid, checks local clearance, and retries with a lateral step when stalled. It can use supported vaults and checked jumps, but will not teleport across an unreachable gap. It does not follow explorer markers. Both encounter toggles can be enabled together; Stop removes the partner and any player combat components added only for training.

### Sparring contact and knockdowns

The partner strikes with its right hand (attack slot 0), right foot (slot 1), or left hand (other slots). Its head, chest, hips, elbows, hands, knees and feet can receive hits. Overlapping regions still count as one target for a single-hit activation. Common humanoid bone names drive these volumes; unmatched regions use a fixed humanoid layout and unmatched striking limbs use the attack's authored reach.

Any four confirmed hits while standing knock the partner down. A melee hit and burst from the same press count once. Heavy hits push harder but do not skip the four-hit requirement. The partner has infinite health by default. After three seconds it stands as soon as there is walkable support and room for its capsule, with posture and the hit count reset. Hits on the downed body push it without restarting recovery. A lethal hit with infinite health disabled still causes a permanent knockout.

## Memory across runs

**Play camera:** Each right-stick click (**R3**) advances one mode: **Free → Lock on to sparring partner → Third-person follow → Free**. Lock-on is skipped if there is no live sparring partner within range (18 units by default). The partner need not be on screen. **V** retains the free/follow toggle.

During a fight, the partner scores the side it actually dodged toward: a confirmed player hit lowers that side’s score; a resolved active swing without contact raises it. It prefers the better side but still tries alternatives and checks the landing. Scores reset on spawn. **Decision replay** includes dodge and counter hit/miss outcomes; a canceled windup is not a successful dodge. A player doubling back during a flank triggers a fresh pursuit route rather than following the stale flank.

The explorer only drops an absent landmark after three consecutive local scans within four units of the old sighting. A closer object is not evidence of disappearance. Forgetting the object does not erase the visited location.

In sparring, press Dodge in the first third of your attack startup to cancel the swing. The partner can sidestep a windup but saves its full dodge for a committed attack; a canceled windup does not grant a recovery counter. When the navigation grid blocks pursuit, the partner tries a reachable side position with an open grid line to the player and periodically updates it as the player moves.

After Stop, open **Decision replay** on the toolbar and scrub through the last 48 timestamped sparring decisions. This is an event history for the most recent session, not a video recording. Explorers also refresh a remembered landmark when they observe its unique name/tag more than 1.5 units from its previous sighting. Duplicate labels are not treated as proof that an object moved.

Enable **View → AI Routes and Dodge Lanes** during Play to draw remaining explorer waypoints in cyan and sparring waypoints in amber. A checked dodge lane flashes green when accepted or red when rejected. A character holding position may have no route to draw. Grid routes no longer cut diagonally past blocked corners. Crossing through a previously stalled waypoint clears that role’s route penalty; backing out on the entry side leaves it intact.

During Play, open **AI decisions** next to **Sparring** to see the last dodge decision, counter status, recent matching swings, and remembered stall count. Repeat the same attack within four seconds and the partner tries a checked diagonal dodge rather than a wide sidestep; switch attack definitions or pause to break the pattern. The attack pattern resets on spawn.

When pursuit stalls, the partner remembers the next waypoint that failed and prefers a detour on its next route. A roaming explorer that stalls en route to a remembered landmark can retry the same destination up to three times with different route costs. Each role retains up to 32 stalled waypoints in its own scene memory. A stall increases route cost but never blocks the only walkable path. Existing memory files remain readable.

Enable either encounter, play, then Stop or exit normally. Next time you play the same scene, each enabled character loads its own memory. The explorer periodically heads back to a visited spot (favoring those with landmarks) and otherwise favors less familiar roaming destinations. The sparring partner still holds its distance; when starting an approach or retreat, it can angle toward a familiar lane or try a less familiar side.

Every 0.75 seconds each AI samples its position and the nearest named or tagged, unparented non-character object within six units. It remembers up to 512 three-unit cells, their visit counts, and the latest nearby sighting per cell. These are observations, not exact movement replays or guarantees that an object is still there.

Memory lives under `.myengine/encounter-memory/<scene-key>/` in the engine working directory, not in the saved scene. Saved scene paths have separate slots; unnamed scenes share a default slot. To reset one AI, stop Play and delete its `explorer.memory` or `sparring.memory` file. A crash before Stop or normal exit loses that session’s new observations.

## Marking a scene

Use empty, unparented entities with a Transform and a **tag** (not a name). Place each marker at the position of a standing character's origin, not inside a collider.

- `ExplorerPOI`: a point to approach and inspect. Roaming explorers scan within 14 world units, pause for 1.5 seconds and remember the last eight selected interests. Follow requests take priority.
- `ExplorerLink:WestStairs`: put this same tag on exactly two markers, one at each end of a straight, physically traversable stairway or ramp. Use a distinct suffix for each pair. The explorer approaches the nearer-height endpoint using the horizontal navigation grid, then steers toward the other endpoint using normal character physics. Links are bidirectional. They do not create geometry, teleport characters, or support elevators. The selector prefers links that move toward the destination's height; it is not a general multi-floor shortest-path solver.

## Play checks

For a horizontal ferry, tag the actual moving deck `ExplorerPlatform` and give it a solid box collider and rigidbody. Its motion system must move the deck and report its world velocity for rider transport. Bring the deck flush with the dock at both ends. The explorer can wait near a gap, step aboard, ride without walking, and step off toward its goal when it detects support beyond the rim. This is local traversal, not ferry scheduling or elevator routing.

In follow mode, block one side of a moving player with a wall, then open it again. The explorer should favor the clear side without immediately switching back. Try a moving box crossing the explorer's route: it should sidestep or yield. Prediction uses short constant-velocity forecasts for solid primitive colliders; mesh geometry still uses the normal physics collision checks.

At a dock, the explorer forecasts up to four seconds of horizontal deck motion using the reported rigidbody velocity. It favors the earliest deck expected to remain within stepping distance long enough to board. It waits until that window opens and releases the target if the deck reverses or stops out of reach. It does not predict a scripted turnaround before the deck's velocity changes.

Place a POI upstairs and a link on the connecting ramp. Check that the explorer uses the ramp and pauses at the POI. Request follow in a narrow corridor with another character ahead: it should slow or yield when the grid has no open side cell. For recovery, temporarily block a route and watch for a lateral escape and replan rather than continuous circling. Check ledges in the actual level; the grid, capsule sizes and physical landing surfaces must agree.
