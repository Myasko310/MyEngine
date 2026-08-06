# Quick launch script for MyEngine in Visual Studio
# Double-click this file to open the project

$solutionPath = Join-Path $PSScriptRoot "build\debug\MyEngine.slnx"

if (Test-Path $solutionPath) {
	Write-Host "Opening MyEngine in Visual Studio..." -ForegroundColor Green
	Start-Process $solutionPath
} else {
	Write-Host "Error: Solution file not found at:" -ForegroundColor Red
	Write-Host $solutionPath
	Write-Host ""
	Write-Host "Please run CMake configuration first:" -ForegroundColor Yellow
	Write-Host "  cmake --preset windows-x64-debug"
	Write-Host "  cmake --build build/debug --config Debug"
	pause
}
