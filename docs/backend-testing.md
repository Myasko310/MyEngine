# Backend Contract Testing (CI-Oriented)

This project provides a dedicated backend contract test executable and a smoke suite.

## Targets

- `AiBackendContractTests`
  - Focused backend contract test (runner + bridge health contract assertions).
- `MyEngineSmokeTests`
  - Broad engine smoke suite including AI backend integration smoke coverage.

## Configure

```powershell
cmake -S . -B out/build/x64-debug-clean -G "Visual Studio 18 2026" -A x64
```

## Build CI-Relevant Test Targets

```powershell
cmake --build out/build/x64-debug-clean --config Debug --target AiBackendContractTests MyEngineSmokeTests
```

## Run Backend Contract Test Only (fast path)

```powershell
ctest --test-dir out/build/x64-debug-clean -C Debug --output-on-failure -R AiBackendContractTests
```

## Run Backend + Smoke Validation

```powershell
ctest --test-dir out/build/x64-debug-clean -C Debug --output-on-failure -R "AiBackendContractTests|MyEngineSmokeTests"
```

## Notes for CI

- `AiBackendContractTests` is deterministic and does not require external model weights.
- Test logic uses a stub command template for success-path verification and validates:
  - structured runner diagnostics (`template_source`, attempts, durations),
  - retry tuning telemetry (`retry_count`, `retry_backoff_ms`, `total_duration_ms`),
  - backend health summary JSON generation and telemetry fields.
- `MyEngineSmokeTests` requires the sample plugin artifact and runtime DLL availability; CMake currently handles these copy/dependency steps.

## Failure Triage

- If only `AiBackendContractTests` fails:
  - inspect latest test output for missing telemetry/contract keys.
- If only `MyEngineSmokeTests` fails:
  - check plugin packaging/runtime setup first, then review failing subtest from `LastTest.log`.
- If both fail early at process launch:
  - verify runtime DLL copy step and working directory assumptions in CMake test setup.
