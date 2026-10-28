param(
	[Parameter(Mandatory = $true)][string]$Prompt,
	[Parameter(Mandatory = $true)][string]$OutputDir,
	[Parameter(Mandatory = $true)][string]$OutputName,
	[Parameter(Mandatory = $false)][string]$Backend = "hunyuan3d",
	[Parameter(Mandatory = $false)][int]$TimeoutMs = 300000
)

$ErrorActionPreference = "Stop"

# -----------------------------------------------------------------------------
# MyEngine text-to-3D command template for Hunyuan3D-style local pipelines.
#
# Expected contract:
# - This script must produce: <OutputDir>/<OutputName>.glb
# - Exit code 0 on success, non-zero on failure.
#
# You must edit the command section below to match your local Hunyuan3D fork.
# -----------------------------------------------------------------------------

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$targetGlb = Join-Path $OutputDir ($OutputName + ".glb")

# Optional scratch paths
$tempImage = Join-Path $OutputDir ($OutputName + "_prior.png")
$tempMesh = Join-Path $OutputDir ($OutputName + "_raw.obj")

Write-Host "[MyEngine][Hunyuan3D] Prompt: $Prompt"
Write-Host "[MyEngine][Hunyuan3D] OutputDir: $OutputDir"
Write-Host "[MyEngine][Hunyuan3D] OutputName: $OutputName"
Write-Host "[MyEngine][Hunyuan3D] Target GLB: $targetGlb"

# -----------------------------------------------------------------------------
# Concrete default command wiring
#
# This template calls a Python entrypoint expected to support:
#   --prompt <text>
#   --output_glb <path>
#   --backend <id>
#   --timeout_ms <int>
#
# Configure these env vars for your machine:
#   HUNYUAN3D_PYTHON  (optional, default: python)
#   HUNYUAN3D_SCRIPT  (optional, default: tools/hunyuan3d/infer_text_to_3d.py)
# -----------------------------------------------------------------------------

$pythonExe = if ($env:HUNYUAN3D_PYTHON) { $env:HUNYUAN3D_PYTHON } else { "python" }
$scriptPath = if ($env:HUNYUAN3D_SCRIPT) { $env:HUNYUAN3D_SCRIPT } else { "tools/hunyuan3d/infer_text_to_3d.py" }

if (-not (Test-Path $scriptPath)) {
    throw "Hunyuan3D script not found at '$scriptPath'. Set HUNYUAN3D_SCRIPT to your local entrypoint path."
}

$arguments = @(
    $scriptPath,
    "--prompt", $Prompt,
    "--output_glb", $targetGlb,
    "--backend", $Backend,
    "--timeout_ms", "$TimeoutMs"
)

Write-Host "[MyEngine][Hunyuan3D] Running: $pythonExe $($arguments -join ' ')"
& $pythonExe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Hunyuan3D command failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $targetGlb)) {
    throw "Hunyuan3D command finished but GLB was not produced: $targetGlb"
}

Write-Host "[MyEngine][Hunyuan3D] GLB generated: $targetGlb"
