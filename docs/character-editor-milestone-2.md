# Character Editor — Milestone 2: Body Sliders + Bone Sockets

Builds on [Milestone 1](character-editor-milestone-1.md) (bone hierarchy, skeleton overlay, bone gizmo, `*.character.json` presets). M2 adds animation-safe body shaping and mesh attachments driven by the same `CharacterCustomizationComponent`.

## What's new

### Body sliders
- `BodySlider { name, value, minValue, maxValue, mappings[] }` where each `BodySliderBoneMapping` targets a bone by name with `positionPerUnit`, `rotationDegreesPerUnit`, `scalePerUnit`.
- `CharacterCustomizationComponent::ComputeEffectiveOverride(bone)` sums slider contributions (positions/rotations additive, scales multiplicative) and composes the manual `BoneOverride` on top. `AnimationSystem::ApplyBoneOverrides` now applies this effective delta, so sliders ride on the sampled/mirrored local pose exactly like M1 overrides — every clip, blend and mirror path keeps working.
- `EnsureDefaultBodySliders()` installs the Mixamo-style set used by the Akaza player: Height, Torso Length, Torso Bulk, Shoulder Width, Arm Length, Arm Thickness, Leg Length, Leg Thickness, Neck Length, Head Size. Bones not present on the skeleton are skipped (the panel greys those sliders out and lists missing bones in the tooltip).
- Main.cpp installs defaults on the Player at startup before loading `assets/characters/player.character.json`.

### Bone sockets
- `BoneSocket { name, boneName, meshPath, positionOffset, rotationOffsetDegrees, scale, visible }` stored on the component.
- `MyEngine::CharacterSocketSystem::Update(scene)` runs right after the render-pass `animationSystem.Update` in main.cpp. For each socket it:
  - creates a runtime entity (`"<Owner>:<Socket>"`, tag `SocketAttachment`) carrying `SocketAttachmentComponent { ownerEntityID, socketName, loadedMeshPath }`;
  - loads the mesh when `meshPath` changes (`primitive_cube` / `primitive_sphere` / `primitive_plane` or any model via `AssetManager::LoadModel`, first mesh) through `AssetManager::AttachMeshToEntity`;
  - writes world TRS = `ownerWorld * boneModel * socketLocal` (bone model transform is `boneMatrices[i] * inverse(offsetMatrix)`);
  - mirrors `visible` (and the component `enabled` flag) onto the renderer;
  - destroys attachments whose owner or socket no longer exists.
- Attachment entities are **not** serialized into scenes (`SceneSerializer` skips `SocketAttachmentComponent`); they are rebuilt from the preset.
- `CharacterSocketSystem::ComputeSocketWorldMatrix` is exposed for the editor overlay.

### Preset format v2
```json
{
  "type": "CharacterPreset",
  "version": 2,
  "enabled": true,
  "boneOverrides": [ ... M1 ... ],
  "bodySliders": [ { "name": "Height", "value": 0.35 } ],
  "sockets": [
	{ "name": "RightHandWeapon", "bone": "RightHand", "mesh": "assets/models/sword.fbx",
	  "positionOffset": [0,0,0], "rotationOffsetDegrees": [0,90,0], "scale": [1,1,1], "visible": true }
  ]
}
```
Only non-zero slider values are written; mappings are not persisted (they come from `EnsureDefaultBodySliders`, which `FromJson` calls so saved values land on real mappings). v1 files load unchanged.

## Editor usage
- **View ▸ Character Editor**. Header now has a **Sockets** overlay toggle (green squares + names at each socket).
- **Body Sliders** section: drag sliders; **Reset Sliders** zeroes them. **Reset All** clears both sliders and manual overrides.
- **Sockets** section: type a name and **Add Socket** (defaults to the selected bone and a 0.1-scale `primitive_cube`), pick a bone from the combo or **Use Selected Bone**, set the mesh path (Enter to apply), adjust TRS/visibility, **Remove Socket**.
- The bone gizmo still edits only the manual override; the slider contribution is factored out of the live pose before decomposition.
- Save/Load use the same preset path box as M1.

## Tests (`tests/CharacterEditorSmokeTests.cpp`)
- `BodySliderCompositionTest` — effective override math, slider → pose through `AnimationSystem`, default factory idempotence.
- `PresetV2RoundTripTest` — sliders/sockets JSON round-trip, zero-slider pruning, v1 compatibility.
- `SocketSystemTest` — headless (injected mesh loader): create, follow bone/owner, reload on path change, no duplicates, orphan cleanup on socket and owner removal.

## Known limitations
- Bone scale propagates to descendants (local delta before accumulation); "Arm Thickness" therefore also thickens hands. Child counter-scaling is M3 material.
- Socket meshes use mesh 0 of the model and the default lit shader (or the model's imported material shader).
- Attachments are plain world-space entities; they do not use `TransformComponent::parentID`, so they are not shown nested under the owner in the hierarchy panel.
- Physics/collision ignores sliders and sockets.

## Roadmap
- **M3**: material slots per socket/body region, preset browser (list/duplicate/delete `*.character.json`), slider mapping editor, child counter-scale option.
