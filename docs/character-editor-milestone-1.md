# Character Editor — Milestone 1

Foundation for a character creator built on the playable character's rig
(Akaza, `assets/models/akaza (2).fbx`, 34 bones). M1 ships the override layer,
the editor panel with hierarchy/overlay/gizmo, and the `*.character.json`
preset format. Sockets (M2) and material slots (M3) build on the same component.

## What's included

| Area | Files |
| --- | --- |
| Override layer | `src/components/CharacterCustomizationComponent.h` |
| Pose integration | `src/systems/AnimationSystem.cpp` (`ApplyBoneOverrides`) |
| Preset I/O | `src/character/CharacterPreset.h/.cpp` |
| Editor panel | `src/editor/panels/CharacterEditorPanel.h/.cpp`, `DebugPanelContexts.h` |
| Wiring | `src/main.cpp` (View menu, panel draw, startup preset apply) |
| Tests | `tests/CharacterEditorSmokeTests.cpp` |

## How the override layer works

Each bone can carry a `BoneOverride { positionOffset, rotationOffsetDegrees, scaleMultiplier }`.
The delta is composed as `T * R * S` and applied in local space **after** the clip
is sampled (and mirrored) but **before** parent accumulation:

```
finalLocal[i] = animatedLocal[i] * delta[i]
model[i]      = model[parent] * finalLocal[i]
palette[i]    = model[i] * offsetMatrix[i]
```

Because the delta is layered on the sampled pose, every existing clip, cross-fade,
mirror path and root-motion rule keeps working unchanged. Children inherit the
parent's delta (so scaling `Spine` lengthens the torso and lifts the head), which is
the behaviour body-proportion sliders will rely on in M2.

During cross-fades the delta is applied to a copy so `AnimationComponent::localPose`
(the next blend's source snapshot) stays customization-free and the delta is never
applied twice.

Overrides are keyed by **bone name**, so a preset survives skeleton reloads and can
be shared across characters with the same naming convention.

## Using the panel

`View > Character Editor` (off by default).

- **Target** – the selected entity if it has a `SkeletonComponent`, otherwise the Player.
- **Bone Hierarchy** – tree built from `Bone::parentIndex`; overridden bones are tinted gold.
- **Skeleton Overlay / Bone Names** – joints and bones projected onto the viewport
  using the live animated pose (`boneMatrices[i] * inverse(offsetMatrix)`).
- **Bone Gizmo** – ImGuizmo manipulator on the selected bone. Uses the toolbar's
  current operation/mode (Translate/Rotate/Scale, World/Local). The animated base is
  snapshotted on drag start so the running animation does not fight the drag. The
  entity transform gizmo is suppressed while the bone gizmo is shown.
- **Sliders** – position / rotation / per-axis & uniform scale for the selected bone,
  plus `Reset Bone`.
- **Enable Overrides** – A/B toggle without losing data.
- **Preset** – path field with `Save`, `Load`, `Reset All`.

## Preset format (`*.character.json`)

```json
{
  "type": "CharacterPreset",
  "version": 1,
  "enabled": true,
  "skeletonBoneCount": 34,
  "boneOverrides": [
	{
	  "bone": "Spine",
	  "positionOffset": [0.0, 0.0, 0.0],
	  "rotationOffsetDegrees": [0.0, 0.0, 0.0],
	  "scaleMultiplier": [1.1, 1.1, 1.1]
	}
  ]
}
```

Identity overrides are not written. Unknown bones are preserved on load so a preset
can be moved between rigs. On boot, `assets/characters/player.character.json` is
applied to the Player if it exists (after the startup scene has loaded).

## Tests

`CharacterEditorSmokeTest` (CPU-only, synthetic 3-bone chain):

- `BoneOverride::BuildDeltaMatrix` composition order.
- Override translation propagates to children; scale lengthens the chain;
  `enabled = false` restores the animated pose; blend-source isolation.
- Preset JSON/file round-trip, sorted output, identity pruning, malformed/typed rejection.

## Known limitations / follow-ups

- Non-uniform bone scale propagates to children (standard skeletal behaviour);
  M2 may add a "compensate children" option for body-shaping sliders.
- `ComputeCharacterCapsuleFromSkeleton` and physics ignore overrides.
- Rotation decomposition from the gizmo uses Euler angles; extreme multi-axis
  rotations may re-express differently in the sliders (pose is unaffected).
- Panel UI is not exercised by the headless smoke test.

## Roadmap

- **M2 – Body shaping & sockets:** named morph sliders (Height, Arm Length, …)
  mapping to bone groups; `BoneSocket { bone, meshAsset, localOffset }` attachments
  rendered as child entities following the bone.
- **M3 – Materials & presets UX:** per-slot material swapping, preset browser,
  thumbnails, apply-to-NPC.
