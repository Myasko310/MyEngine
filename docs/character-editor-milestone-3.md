# Character Editor — Milestone 3 (part 1): Slider Mapping Editor

## What's new
- **Slider mapping editor** in `View > Character Editor > Body Sliders`. Toggle **Mapping Editor** to reveal a radio button next to every slider; pick one to edit it.
- Per slider: rename, edit **Range (min/max)**, **Reset To Default Mapping** (default sliders only), **Remove Slider**.
- Per mapping: choose the driven bone (combo or **Use Selected Bone** from the hierarchy), and edit **Position / unit**, **Rotation (deg) / unit**, **Scale / unit**. Selecting a mapping also selects its bone in the hierarchy/overlay. Missing bones are greyed.
- **Add Slider** creates an empty user-defined slider (name auto-suffixed on collision). **Add Mapping** targets the currently selected hierarchy bone.
- Edited sliders are shown with a `*` suffix and are tagged `customMappings`; `EnsureDefaultBodySliders()` no longer overwrites them.

## Data model (`src/components/CharacterCustomizationComponent.h`)
- `BodySlider::customMappings` — set by any mapping/range/name edit or by `AddCustomSlider()`.
- `AddCustomSlider(name)`, `RemoveSlider(name)`, `ResetSliderToDefault(name)`.

## Preset format v3 (`src/character/CharacterPreset.cpp`)
- Default sliders still persist only `{ "name", "value" }` (zero values omitted).
- Custom sliders persist their full definition:
  `{ "name", "value", "custom": true, "min", "max", "mappings": [{ "bone", "positionPerUnit", "rotationDegreesPerUnit", "scalePerUnit" }] }`
- v1/v2 presets load unchanged. Loading a preset clears any previously loaded custom slider definitions that the new preset does not define.

## Tests
`SliderMappingEditorTest` in `tests/CharacterEditorSmokeTests.cpp`: custom slider creation/rejection, default-slider customization surviving `EnsureDefaultBodySliders`, v3 JSON round-trip, stale custom definitions dropped on load, reset-to-default and removal.

## Preset browser
- In the **Preset** section toggle **Browser** to show a list of `*.character.json` files in a folder (default `assets/characters`; edit the path and press Enter or **Refresh** to rescan).
- The currently applied preset is highlighted `(active)`. Single-click selects and fills the path/name fields; **double-click loads**.
- Actions on the selected preset: **Load**, **Overwrite** (save current customization into it), **Duplicate** (uses the name field, or auto `" Copy"/" Copy N"` when unchanged), **Rename**, **Delete** (confirmation modal).
- **Save As** writes the current customization to `<folder>/<name>.character.json`.
- API in `MyEngine::CharacterPreset`: `ListPresets`, `PresetDisplayName`, `MakePresetPath`, `Duplicate`, `Rename`, `Delete` (refuses non-`.character.json` files). Covered by `PresetLibraryTest`.

## Child counter-scale
- `BoneOverride::counterScaleChildren` and `BodySliderBoneMapping::counterScaleChildren` ("Counter-scale children" checkbox in the bone editor and the mapping editor).
- When set, the bone's scale is accumulated into `childCounterScale` and `AnimationSystem::ApplyBoneOverrides` prepends `S(1 / childCounterScale)` to each direct child's local transform, so the child (and its subtree) keeps its size while still following the scaled parent's position.
- Default **Arm Thickness** / **Leg Thickness** sliders now use it, so hands and feet are no longer thickened.
- Serialized as an optional `"counterScaleChildren": true` on bone overrides and custom mappings. Covered by `CounterScaleChildrenTest`.

## Body and socket material customization
- Open **Character > Open Character Editor (Player)**, then expand **Body Material**, or select a socket and expand **Socket Material**.
- Enable material customization for that slot. **Tint** multiplies the existing color/texture. Optional **Shininess** controls lit highlights; **Override PBR Surface** exposes metallic, roughness and emissive values for an already PBR-enabled material/shader.
- Without asset overrides, texture maps and shaders remain unchanged. Overrides are applied at draw time without modifying shared assets or original renderer assignments, so characters sharing a material can have different appearances.
- **Reset Material** restores that slot's original appearance. The global **Enable Overrides** toggle bypasses body customization (and retains the existing behavior of hiding sockets). **Reset All** continues to reset posing/sliders only.
- Save through the existing preset controls. Optional v3 `bodyMaterial` and socket `material` objects preserve enabled, tint, overrideShininess, shininess, overridePBR, metallic, roughness and emissive fields, including disabled edits. Older presets reset material settings to disabled defaults.
- Invalid material types, non-finite numbers and out-of-range values are rejected before changing the current customization.
- `CharacterMaterialTest` covers scalar application, disabled behavior, slot independence, preset round-trip, legacy defaults and rejection without mutation. Visual shader behavior still requires an interactive preview check.

## Focus and orbit preview camera
- In edit mode, select **Character Editor Test** (or another skinned character) in Scene Hierarchy, open **View > Character Editor**, and press **Preview Camera > Focus Character**.
- Preview uses the main viewport without modifying any scene camera or character transforms. Rig bounds are framed with padding and the camera refocuses when the target changes. Use Focus again after major posing/scale changes.
- Drag inside **Drag to orbit / wheel to zoom** to orbit; scroll while hovering that control to zoom. The viewport itself remains available for bone posing and selection.
- **Front / Back / Side**, orbit/elevation/distance sliders and **Vertical Framing** provide precise framing. Front is the world +Z view, not a model-specific facing direction.
- **Return to Scene Camera**, closing/hiding the editor, or entering Play exits preview. Gameplay camera controls remain unchanged. Preview state is editor-only and is not saved in presets or scenes.
- Controls apply on the next frame so scene rendering, picking, skeleton overlays and gizmos share the same camera matrices.
- `CharacterPreviewCameraTest` covers framing at different aspect ratios, orbit/zoom limits and finite degenerate-bound matrices; ImGui builds also test target switching and exit conditions.
- This is not an isolated render-to-texture preview. Joint bounds approximate the silhouette and do not include socket meshes; interactive framing/input behavior still needs a visual check.

## Character picker
- Open **View > Character Editor** and use the **Character** dropdown at the top to switch between **Player**, **Character Editor Test**, and other loaded rigs. IDs distinguish duplicate names; empty/unloaded skeletons are excluded.
- Picking a character updates Scene Hierarchy selection. Existing hierarchy selection and the Player menu/shortcut still work.
- Switching clears bone/socket/mapping selection and gizmo drag state, and refocuses an active preview on the next frame. Character customization itself is preserved.
- Each target restores its last loaded/saved preset path. Unsaved non-Player characters start with `assets/characters/character_<entityID>.character.json`, rather than the Player's preset path. Explicitly choosing a preset in the browser can still overwrite it.
- `CharacterPickerTest` covers switching, invalid/deleted targets, target-specific resets and preset path isolation in ImGui builds.

## Material and base-color texture assets
- **Material Selection** lists `.material.json` files recursively under the resolved project assets folder for both body and socket materials. Choose an asset to enable customization, or **Original / Inherit** to restore the original material. Use **Refresh Material List** after creating/importing assets; Browse remains available for external files. An empty list means no matching assets exist; create one using Material Browser or browse to an existing file.
- Within **Body Material** or **Socket Material**, enable customization and use **Material Asset > Browse...** for a `.material.json` asset or **Base Color Texture > Browse...** for an image. Paths can also be entered manually; press Enter to apply.
- A replacement material supplies surface values, maps, shader, render queue and render flags. Existing renderer map overrides do not mask the selected material. An explicit base-color texture wins over its albedo texture, for both lit and PBR rendering; other maps remain intact.
- **Clear** restores inheritance for that field. Disable customization or **Reset Material** to restore the original appearance. Shared materials and renderer assignments are not modified.
- Missing/undecodable assets fall back to the original material or inherited texture. The panel shows load errors; **Retry Asset Loads** retries failed references. Successful resources remain managed by the engine asset cache (this is not a hot-reload command).
- Paths beneath the resolved assets directory become portable `assets/...` paths; external files retain absolute paths and are not copied into the project.
- Presets persist optional `materialPath` and `albedoTexturePath` strings, including when disabled. Older presets inherit the original assets. Runtime load results are not serialized.
- Skinned characters retain a skinning-capable vertex shader while using the selected material's fragment shader. Use materials compatible with the engine mesh vertex interface; arbitrary custom shaders may require additional integration. Image selection changes base color only, not UVs or other individual texture maps.
- `CharacterAssetSelectionTest` covers cached resolution, failure/retry/clear, path persistence and invalid-input rejection. File-dialog and rendered appearance checks still require interactive validation.

## Body slider rig compatibility
- Default sliders bind bare names such as `Hips` to unique namespaced bones such as `mixamorig:Hips` in the loaded rig. This fixes the Akaza sliders previously resolving no bones and producing no deformation.
- Binding runs during animation evaluation, including paused poses and loaded presets, and before slider UI diagnostics. Custom mappings stay exact; ambiguous matches are not guessed. Unmatched sliders show **No matching bones**; disabled customization shows an **Enable Overrides** reminder.
- `PrefixedBodySliderTest` checks actual paused-pose deformation, preset rebinding and mapping safety. `CharacterMaterialListTest` checks nested material discovery and extension filtering.

## Remaining material work
- Separate body regions require mesh material slots or texture masks; the current body slot affects the entire character mesh.
- Individual normal/roughness/AO map pickers and asset hot reload are not included; these maps can be supplied through a material asset.
