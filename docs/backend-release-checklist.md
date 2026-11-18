# Backend Release Checklist

Use this checklist before marking backend implementation as release-ready.

## Contract Integrity

- [ ] Runner contract JSON includes: `success`, `message`, `attempt_count`, `attempts`, `total_duration_ms`
- [ ] Runner retry tuning fields are present: `retry_count`, `retry_backoff_ms`
- [ ] Runner fallback metadata fields are present: `backend`, `backend_fallback_chain`
- [ ] Bridge health summary JSON includes: `success`, `mode`, `template_count`, `duration_ms`, `message`
- [ ] Provider messages surface template source + backend + backend_chain + attempts + duration telemetry

## Editor UX

- [ ] Panel preflight passes for configured backend
- [ ] Auto backend self-check runs on panel open (text-to-3D provider)
- [ ] Backend Health Summary section updates after manual/auto checks
- [ ] Retry Count and Retry Backoff controls are visible and applied
- [ ] Backend fallback chain input is visible and applied
- [ ] Telemetry history artifact operations work (open/reload/clear) and persist to JSON
- [ ] Auto-recovery actions apply session env fixes (`MYENGINE_TEXT_TO_3D_COMMAND`, `HUNYUAN3D_SCRIPT`)

## Test Coverage

- [ ] `AiBackendContractTests` passes
- [ ] `MyEngineSmokeTests` passes
- [ ] Contract test validates success path + health JSON + telemetry keys
- [ ] Contract test validates fallback metadata in diagnostics and runner JSON (`backend`, `backend_chain`/`backend_fallback_chain`)

## Build and Runtime

- [ ] CMake configure succeeds on clean directory
- [ ] Debug build succeeds
- [ ] Runtime DLL copy steps are intact for test executables
- [ ] Sample plugin dependency path is valid for smoke runs

## Documentation

- [ ] `docs/ai-text-to-3d-setup.md` reflects latest panel workflow and telemetry
- [ ] `docs/backend-testing.md` includes CI commands and triage notes
- [ ] Environment variables (`MYENGINE_TEXT_TO_3D_COMMAND`, `HUNYUAN3D_*`) documented
