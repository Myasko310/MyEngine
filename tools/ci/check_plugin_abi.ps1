param(
	[string]$ManifestPath = "",
	[string]$PluginDllPath = "",
	[switch]$ExpectFailure
)

$ErrorActionPreference = 'Stop'

function Invoke-PluginAbiValidation {
	$sdkHeader = Get-Content "src/plugins/PluginSDK.h" -Raw
	if ($sdkHeader -notmatch 'kPluginSdkVersionMajor\s*=\s*(\d+)') { throw "Failed to parse SDK major from PluginSDK.h" }
	$engineSdkMajor = [int]$Matches[1]
	if ($sdkHeader -notmatch 'kPluginSdkVersionMinor\s*=\s*(\d+)') { throw "Failed to parse SDK minor from PluginSDK.h" }
	$engineSdkMinor = [int]$Matches[1]

	$resolvedManifestPath = $ManifestPath
	$resolvedPluginDllPath = $PluginDllPath

	if ([string]::IsNullOrWhiteSpace($resolvedManifestPath) -or [string]::IsNullOrWhiteSpace($resolvedPluginDllPath)) {
		$packageDirCandidates = @(
			"build/Debug/plugins/SamplePlugin",
			"build/debug/Debug/plugins/SamplePlugin"
		)

		$packageDir = $null
		foreach ($candidate in $packageDirCandidates) {
			if (Test-Path $candidate) { $packageDir = $candidate; break }
		}

		if (-not $packageDir) { throw "Sample plugin package directory not found in expected output locations." }

		if ([string]::IsNullOrWhiteSpace($resolvedManifestPath)) {
			$resolvedManifestPath = Join-Path $packageDir "plugin.ini"
		}
		if ([string]::IsNullOrWhiteSpace($resolvedPluginDllPath)) {
			$resolvedPluginDllPath = Join-Path $packageDir "SamplePlugin.dll"
		}
	}

	if (!(Test-Path $resolvedManifestPath)) { throw "Plugin manifest missing: $resolvedManifestPath" }
	if (!(Test-Path $resolvedPluginDllPath)) { throw "Plugin DLL missing: $resolvedPluginDllPath" }

	$manifest = @{}
	foreach ($line in Get-Content $resolvedManifestPath) {
		$trimmed = $line.Trim()
		if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#") -or $trimmed.StartsWith(";")) { continue }
		$parts = $trimmed -split '=', 2
		if ($parts.Count -eq 2) { $manifest[$parts[0].Trim()] = $parts[1].Trim() }
	}

	if (-not $manifest.ContainsKey('name')) { throw "Manifest missing 'name'" }
	if (-not $manifest.ContainsKey('dll')) { throw "Manifest missing 'dll'" }
	if (-not $manifest.ContainsKey('sdk_major')) { throw "Manifest missing 'sdk_major'" }
	if (-not $manifest.ContainsKey('sdk_minor')) { throw "Manifest missing 'sdk_minor'" }

	$pluginSdkMajor = [int]$manifest['sdk_major']
	$pluginSdkMinor = [int]$manifest['sdk_minor']

	if ($pluginSdkMajor -ne $engineSdkMajor) {
		throw "Plugin SDK major mismatch. Plugin=$pluginSdkMajor Engine=$engineSdkMajor"
	}

	if ($pluginSdkMinor -gt $engineSdkMinor) {
		throw "Plugin SDK minor is newer than engine. Plugin=$pluginSdkMinor Engine=$engineSdkMinor"
	}

	if ($manifest['dll'] -ne 'SamplePlugin.dll') {
		throw "Manifest DLL name mismatch. Expected SamplePlugin.dll, got $($manifest['dll'])"
	}
}

$validationFailed = $false
try {
	Invoke-PluginAbiValidation
}
catch {
	$validationFailed = $true
	if (-not $ExpectFailure) {
		throw
	}
	Write-Host "Expected ABI validation failure observed: $($_.Exception.Message)"
}

if ($ExpectFailure -and -not $validationFailed) {
	throw "Expected ABI validation to fail, but it passed."
}
