# AI Text-to-3D Local Setup (GLB)

This project supports real local text-to-3D generation through provider:

- `local_text_to_3d_glb_v1`

The provider calls:

- `tools/ai/generate_text_to_3d.py` (runner contract)
- which in turn executes command from environment variable `MYENGINE_TEXT_TO_3D_COMMAND`

## 1) Prepare your local Hunyuan3D environment

Use your preferred Hunyuan3D fork/runtime and ensure you can run it from terminal first.

Requirements (typical):

- Python environment with your Hunyuan3D dependencies installed
- Model weights downloaded for your chosen fork
- Ability to export a `.glb`
- A runnable backend command configured via one of:
  - `HUNYUAN3D_INFER_COMMAND_TEMPLATE`
  - `HUNYUAN3D_REPO`

## 2) Configure command template

The template now includes a concrete Python command and uses:

- `HUNYUAN3D_PYTHON` (optional, default `python`)
- `HUNYUAN3D_SCRIPT` (optional, default `tools/hunyuan3d/infer_text_to_3d.py`)

The bundled `tools/hunyuan3d/infer_text_to_3d.py` is a backend bridge. Configure it with either:

- `HUNYUAN3D_INFER_COMMAND_TEMPLATE` (full command template), or
- `HUNYUAN3D_REPO` (path to your local Hunyuan3D repo; bridge will try common infer entrypoints).

Wave 4 telemetry features:
- runner contract includes `attempt_count`, `attempts[].duration_ms`, `retry_count`, `retry_backoff_ms`, `total_duration_ms`
- bridge health summary includes `duration_ms`

Set these to your local environment if needed.

Contract requirements:

- Input params come as PowerShell arguments:
  - `-Prompt`
  - `-OutputDir`
  - `-OutputName`
  - `-Backend`
  - `-TimeoutMs`
- Script must output final GLB at:
  - `<OutputDir>/<OutputName>.glb`
- Exit code must be `0` on success.

## 3) Set environment variable for MyEngine runner

Set `MYENGINE_TEXT_TO_3D_COMMAND` so runner can invoke your template script.

On Windows, if `MYENGINE_TEXT_TO_3D_COMMAND` is not set, the runner will automatically fall back to:

- `tools/ai/hunyuan3d_command_template.ps1`

(using a default PowerShell invocation).

PowerShell (current session):

```powershell
$env:MYENGINE_TEXT_TO_3D_COMMAND = "powershell -ExecutionPolicy Bypass -File tools/ai/hunyuan3d_command_template.ps1 -Prompt \"{prompt}\" -OutputDir \"{output_dir}\" -OutputName \"{output_name}\" -Backend \"{backend}\" -TimeoutMs {timeout_ms}"
```

PowerShell (persistent for user):

```powershell
setx MYENGINE_TEXT_TO_3D_COMMAND "powershell -ExecutionPolicy Bypass -File \"tools/ai/hunyuan3d_command_template.ps1\" -Prompt \"{prompt}\" -OutputDir \"{output_dir}\" -OutputName \"{output_name}\" -Backend \"{backend}\" -TimeoutMs {timeout_ms}"
```

Optional (if your Python/script paths are custom):

```powershell
setx HUNYUAN3D_PYTHON "C:\\path\\to\\python.exe"
setx HUNYUAN3D_SCRIPT "C:\\path\\to\\hunyuan3d\\infer_text_to_3d.py"
```

Configure backend bridge (choose one):

```powershell
setx HUNYUAN3D_REPO "C:\\path\\to\\Hunyuan3D"
```

or

```powershell
setx HUNYUAN3D_INFER_COMMAND_TEMPLATE "{python} C:\\path\\to\\infer.py --prompt {prompt_q} --output_glb {output_glb_q}"
```

Then restart Visual Studio so it picks up new environment variables.

## 4) Use in editor

1. Open **AI Asset Generator** panel.
2. Select provider `local_text_to_3d_glb_v1`.
3. Click **Run Preflight Test**.
4. Click **Test Runner Command**.
5. Click **Test Backend Bridge** (must pass before generation).
6. Review **Backend Health Summary** section (status, mode, template count, duration).
7. Tune **Retry Count** and **Retry Backoff (ms)** if needed.
8. Enter prompt and output name.
9. Click **Generate AI Asset**.

On success, generated files are queued into Asset Browser import pipeline and appear under:

- `assets/generated/ai/...`

## 5) Troubleshooting

### Error: "No text-to-3D backend command configured"
On Windows, ensure `tools/ai/hunyuan3d_command_template.ps1` exists; otherwise set `MYENGINE_TEXT_TO_3D_COMMAND` and restart the editor.
On non-Windows, set `MYENGINE_TEXT_TO_3D_COMMAND` explicitly.

### Error: "runner script not found"
Ensure `tools/ai/generate_text_to_3d.py` exists in repo and current working directory resolves repo root ancestry.

### Error mentions Hunyuan3D backend not configured
Set one of:
- `HUNYUAN3D_REPO`
- `HUNYUAN3D_INFER_COMMAND_TEMPLATE`

Then run:

```powershell
python tools/hunyuan3d/infer_text_to_3d.py --self_check
```

Expected output includes mode and template count. If it fails, fix env setup first.

You can also export structured health JSON:

```powershell
python tools/hunyuan3d/infer_text_to_3d.py --self_check --health_json tools/hunyuan3d/backend_health_summary.json
```

### Error includes `template_source=` / `command=` / `stderr=` / `attempts=` / `duration_ms=`
This is structured backend diagnostics from the runner contract. Use it to pinpoint whether failure is:
- wrong template source (`default` vs `env`),
- broken backend command path,
- repeated fallback retry failures (`attempts=` / `retry_count`),
- overly aggressive or insufficient backoff (`retry_backoff_ms`),
- slow backend execution (`duration_ms=` / `total_duration_ms`), or
- backend runtime/import/weights failure reported on stderr.

### Error: "GLB was not found at expected path"
Your backend command succeeded but didn’t write `<OutputDir>/<OutputName>.glb`. Fix export path in your Hunyuan infer command.

### Spawn issues
In Asset Browser, double-click the generated `.glb` file under `assets/generated/ai/...`.
