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

## Remaining M3 roadmap
- Material slots per socket/body region
