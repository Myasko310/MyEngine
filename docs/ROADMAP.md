# MyEngine Roadmap

## Planning Principles
- Milestone-based delivery with explicit review gates.
- Prioritize authoritative server/client networking first for major engine work.
- Use a hybrid plugin SDK model (static core + optional dynamic plugins).
- Maintain a performance target of 120 FPS on high-end GPUs.
- Require an automated test matrix (including performance baselines) before milestone completion.

## Milestone 0 (Now): Prefab Workflow Stability
### Scope
- Expand prefab override parity beyond the current subset.
- Start with AudioSource and AudioListener override support across detect/revert/apply and metadata persistence.
- Add smoke-test validation for new prefab override metadata fields.

### Review Gate
- Override detection lists audio differences correctly.
- Revert selected override restores source prefab audio values.
- Apply-all writes audio overrides back to source prefab.
- Serialization roundtrip preserves new override flags.

## Milestone 1: Authoritative Networking Foundation
### Scope
- Introduce server-authoritative simulation boundaries for transform + gameplay state replication.
- Define deterministic snapshot + reconciliation flow for client prediction.
- Add networking feature flags and test harness scenes.

### Review Gate
- End-to-end host/client sync demo scene.
- Replication consistency checks pass under induced latency.
- Automated tests cover core replication paths.

## Milestone 2: Plugin SDK Baseline
### Scope
- Publish static core extension API.
- Add optional dynamic plugin loading path with versioned ABI checks.
- Provide sample plugins and integration tests.

### Review Gate
- Static and dynamic plugin samples load and execute.
- API/ABI compatibility checks enforced in CI.

## Milestone 3: Renderer and Runtime Performance Track
### Scope
- Add repeatable performance baseline scenes.
- Track frame time, render passes, and system timings in automated runs.
- Optimize hot paths to sustain 120 FPS target on high-end GPUs.

### Review Gate
- Performance matrix reports generated in CI.
- Baseline thresholds documented and enforced.

## Milestone 4: Test Matrix Completion
### Scope
- Expand smoke/system tests (physics, navmesh, terrain, particle, audio lifecycle, animation state machine runtime).
- Add backend coverage checks and regression suites.

### Review Gate
- Full automated test matrix green.
- Performance baseline checks green.

## Current Sprint Kickoff Tasks
1. Add `overrideAudioSource` and `overrideAudioListener` metadata flags.
2. Serialize/deserialize the new flags in scene and prefab metadata.
3. Extend editor prefab override detect/revert/apply logic for AudioSource/AudioListener.
4. Extend smoke tests for variant metadata roundtrip coverage.