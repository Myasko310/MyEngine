# Generates simple PCM16 mono WAV test tones for exercising the audio engine.
# Run from anywhere; writes files into assets/audio relative to the repo root.

param(
	[string]$OutDir = (Join-Path $PSScriptRoot "..\..\assets\audio")
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Write-WavTone {
	param(
		[string]$Path,
		[double]$FrequencyHz,
		[double]$DurationSeconds,
		[int]$SampleRate = 44100,
		[double]$Amplitude = 0.5,
		[double]$FadeSeconds = 0.02
	)

	$sampleCount = [int]($SampleRate * $DurationSeconds)
	$bytesPerSample = 2
	$channels = 1
	$dataSize = $sampleCount * $bytesPerSample * $channels
	$byteRate = $SampleRate * $channels * $bytesPerSample
	$blockAlign = $channels * $bytesPerSample

	$stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
	$writer = New-Object System.IO.BinaryWriter($stream)

	# RIFF header
	$writer.Write([char[]]"RIFF")
	$writer.Write([int32](36 + $dataSize))
	$writer.Write([char[]]"WAVE")

	# fmt chunk
	$writer.Write([char[]]"fmt ")
	$writer.Write([int32]16)
	$writer.Write([int16]1)        # PCM
	$writer.Write([int16]$channels)
	$writer.Write([int32]$SampleRate)
	$writer.Write([int32]$byteRate)
	$writer.Write([int16]$blockAlign)
	$writer.Write([int16]($bytesPerSample * 8))

	# data chunk
	$writer.Write([char[]]"data")
	$writer.Write([int32]$dataSize)

	$fadeSamples = [int]($SampleRate * $FadeSeconds)
	for ($i = 0; $i -lt $sampleCount; $i++) {
		$t = $i / $SampleRate
		$envelope = 1.0
		if ($i -lt $fadeSamples) { $envelope = $i / [double]$fadeSamples }
		elseif ($i -gt ($sampleCount - $fadeSamples)) { $envelope = ($sampleCount - $i) / [double]$fadeSamples }

		$sampleValue = [Math]::Sin(2 * [Math]::PI * $FrequencyHz * $t) * $Amplitude * $envelope
		$intSample = [int16]([Math]::Round($sampleValue * 32767))
		$writer.Write($intSample)
	}

	$writer.Flush()
	$writer.Close()
	$stream.Close()

	Write-Host "Wrote $Path"
}

# A short one-shot "ping" (e.g. UI click / interact sound)
Write-WavTone -Path (Join-Path $OutDir "ping.wav") -FrequencyHz 880 -DurationSeconds 0.2 -Amplitude 0.6

# A mid-length A4 tone, useful for spatial/3D audio source testing
Write-WavTone -Path (Join-Path $OutDir "tone_440.wav") -FrequencyHz 440 -DurationSeconds 1.5 -Amplitude 0.5

# A lower, longer hum suitable for looping background/ambient testing
Write-WavTone -Path (Join-Path $OutDir "hum_loop.wav") -FrequencyHz 110 -DurationSeconds 2.0 -Amplitude 0.4

# A higher, short blip for a second distinguishable positional source
Write-WavTone -Path (Join-Path $OutDir "blip_high.wav") -FrequencyHz 1760 -DurationSeconds 0.15 -Amplitude 0.5

Write-Host "Done. Test audio clips generated in $OutDir"
