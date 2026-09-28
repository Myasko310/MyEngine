# Appends the "ArenaMechanics" set (elevators, sliding bridge, crushers, sweepers, fire vents)
# to the startup arena scene. Re-running replaces previously generated ArenaMechanics entities.
param([string]$ScenePath = "$PSScriptRoot/../assets/models/akaza animations/akaza player2.json")

$ErrorActionPreference = 'Stop'
$scene = Get-Content $ScenePath -Raw | ConvertFrom-Json
$kept = @($scene.entities | Where-Object { $_.tag -ne 'ArenaMechanics' })
$nextId = [int](($kept | Measure-Object -Property id -Maximum).Maximum) + 1
$tag = 'ArenaMechanics'

function New-Base([string]$name, [double[]]$pos, [double[]]$scale, [double[]]$albedo) {
	$e = [ordered]@{
		id = $script:nextId; name = $name; tag = $tag; layer = 0
		Transform = [ordered]@{ position = $pos; rotation = @(0.0, 0.0, 0.0); scale = $scale; parentID = 0 }
		MeshRenderer = [ordered]@{ visible = $true; albedo = $albedo; shininess = 32.0; useTexture = $false
			shaderVertexPath = 'shaders/arena_lit.vert'; shaderFragmentPath = 'shaders/lit.frag' }
		MeshComponent = [ordered]@{ assetPath = 'primitive_cube'; isSkinned = $false }
	}
	$script:nextId++
	return $e
}

function Add-Solid($e) {
	$e.Rigidbody = [ordered]@{ velocity = @(0.0, 0.0, 0.0); mass = 1.0; drag = 0.01; bounciness = 0.0
		useGravity = $false; gravityScale = 1.0; isKinematic = $true
		freezePositionX = $false; freezePositionY = $false; freezePositionZ = $false; useCCD = $false }
	$e.BoxCollider = [ordered]@{ center = @(0.0, 0.0, 0.0); halfExtents = @(0.5, 0.5, 0.5); isTrigger = $false }
	$e.BoundingSphere = [ordered]@{ center = @(0.0, 0.0, 0.0); radius = 0.8660254; isTrigger = $false }
	return $e
}

function Add-Decorative($e) {
	$e.BoundingSphere = [ordered]@{ center = @(0.0, 0.0, 0.0); radius = 0.8660254; isTrigger = $true }
	return $e
}

function Add-Mover($e, [int]$type, [double[]]$from, [double[]]$to, [double]$speed, [double]$wait) {
	$e.MovingPlatformComponent = [ordered]@{ type = $type; active = $true; pingPong = $true; autoReturn = $true
		startPosition = $from; endPosition = $to; speed = $speed; waitTime = $wait }
	return $e
}

$new = New-Object System.Collections.Generic.List[object]

# Elevators (type 2) lifting the player onto the sliding sky bridge.
$new.Add((Add-Mover (Add-Solid (New-Base 'Mech_WestElevator' @(-12.0, 0.15, -40.0) @(3.0, 0.3, 3.0) @(0.35, 0.42, 0.5))) 2 @(-12.0, 0.15, -40.0) @(-12.0, 4.0, -40.0) 1.6 1.5))
$new.Add((Add-Mover (Add-Solid (New-Base 'Mech_EastElevator' @(12.0, 4.0, -40.0) @(3.0, 0.3, 3.0) @(0.35, 0.42, 0.5))) 2 @(12.0, 4.0, -40.0) @(12.0, 0.15, -40.0) 1.6 1.5))
$new.Add((Add-Solid (New-Base 'Mech_WestElevatorShaftA' @(-13.7, 2.5, -40.0) @(0.3, 5.0, 0.3) @(0.25, 0.22, 0.2))))
$new.Add((Add-Solid (New-Base 'Mech_EastElevatorShaftA' @(13.7, 2.5, -40.0) @(0.3, 5.0, 0.3) @(0.25, 0.22, 0.2))))

# Sliding sky bridge (type 0) shuttling between the elevator tops.
$new.Add((Add-Mover (Add-Solid (New-Base 'Mech_SkyBridge' @(-8.5, 4.0, -40.0) @(3.5, 0.3, 3.0) @(0.55, 0.4, 0.25))) 0 @(-8.5, 4.0, -40.0) @(8.5, 4.0, -40.0) 2.2 1.0))

# Crusher pistons (type 1) slamming down beside the side lanes.
$new.Add((Add-Mover (Add-Solid (New-Base 'Mech_CrusherWest' @(-16.0, 3.6, -8.0) @(2.4, 1.2, 2.4) @(0.45, 0.12, 0.1))) 1 @(-16.0, 3.6, -8.0) @(-16.0, 0.6, -8.0) 5.0 1.2))
$new.Add((Add-Mover (Add-Solid (New-Base 'Mech_CrusherEast' @(16.0, 0.6, -8.0) @(2.4, 1.2, 2.4) @(0.45, 0.12, 0.1))) 1 @(16.0, 0.6, -8.0) @(16.0, 3.6, -8.0) 5.0 1.2))

# Central spinning sweeper hub with two counter-rotating arms.
$new.Add((Add-Solid (New-Base 'Mech_SweeperHub' @(0.0, 0.9, -22.0) @(1.4, 1.8, 1.4) @(0.3, 0.28, 0.32))))
$arm = Add-Decorative (New-Base 'Mech_SweeperArmLow' @(0.0, 0.35, -22.0) @(14.0, 0.3, 0.4) @(0.85, 0.55, 0.1))
$arm.Script = [ordered]@{ scriptPath = 'assets/scripts/arena_sweeper.lua'; enabled = $true; autoStart = $true }
$new.Add($arm)
$arm = Add-Decorative (New-Base 'Mech_SweeperArmHigh' @(0.0, 1.5, -22.0) @(10.0, 0.25, 0.3) @(0.85, 0.2, 0.1))
$arm.Script = [ordered]@{ scriptPath = 'assets/scripts/arena_sweeper_reverse.lua'; enabled = $true; autoStart = $true }
$new.Add($arm)

# Fire vents: dark grates with additive flame particles and a warm point light.
$vents = @(@(-14.0, -18.0), @(14.0, -18.0), @(0.0, -4.0), @(0.0, -34.0))
$i = 0
foreach ($v in $vents) {
	$i++
	$vent = Add-Decorative (New-Base "Mech_FireVent$i" @($v[0], 0.03, $v[1]) @(1.6, 0.06, 1.6) @(0.08, 0.07, 0.07))
	$vent.ParticleEmitter = [ordered]@{ maxParticles = 220; spawnRate = 70.0; emitting = $true; shape = 2
		shapeRadius = 0.5; shapeExtents = @(0.6, 0.05, 0.6); shapeHeight = 1.0
		colorStart = @(1.0, 0.65, 0.15, 1.0); colorEnd = @(0.9, 0.1, 0.0, 0.0)
		sizeStart = 0.35; sizeEnd = 0.05; lifetime = 1.1; lifetimeVariance = 0.4
		emitDirection = @(0.0, 1.0, 0.0); emitSpeed = 3.2; emitSpeedVariance = 1.0; spreadAngle = 14.0
		gravity = @(0.0, 0.6, 0.0) }
	$new.Add($vent)

	$light = [ordered]@{ id = $nextId; name = "Mech_FireVentLight$i"; tag = $tag; layer = 0
		Transform = [ordered]@{ position = @($v[0], 1.2, $v[1]); rotation = @(0.0, 0.0, 0.0); scale = @(1.0, 1.0, 1.0); parentID = 0 }
		Light = [ordered]@{ type = 1; color = @(1.0, 0.55, 0.2); intensity = 2.2; direction = @(0.0, -1.0, 0.0)
			position = @($v[0], 1.2, $v[1]); range = 9.0; innerCone = 12.5; outerCone = 17.5; shadowBias = 0.005
			castShadows = $false; pointShadowSizeOverride = 0; pointShadowPCFSamplesOverride = 0
			pointShadowPCFRadiusOverride = -1.0; spotShadowSizeOverride = 0; spotShadowPCFRadiusOverride = -1.0 } }
	$nextId++
	$new.Add($light)
}

$all = [object[]](@($kept) + @($new.ToArray()))
$scene.PSObject.Properties.Remove('entities')
$globals = $scene.globalScripts
$scene.PSObject.Properties.Remove('globalScripts')
$scene | Add-Member -NotePropertyName entities -NotePropertyValue $all
$scene | Add-Member -NotePropertyName globalScripts -NotePropertyValue $globals
$json = $scene | ConvertTo-Json -Depth 32
[System.IO.File]::WriteAllText((Resolve-Path $ScenePath), $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Added $($new.Count) ArenaMechanics entities; scene now has $($scene.entities.Count) entities."
