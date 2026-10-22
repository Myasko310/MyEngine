#pragma once

#include <string>
#include <vector>

#include "components/CharacterCustomizationComponent.h"

namespace MyEngine
{
	class Skeleton;
}

// *.character.json preset: persists the per-bone customization layer so a
// character look can be re-applied to any entity sharing the same bone names.
namespace MyEngine::CharacterPreset
{
	constexpr const char* kFileExtension = ".character.json";
	constexpr const char* kDefaultPlayerPresetPath = "assets/characters/player.character.json";

	// Serializes the component to JSON text (pretty-printed).
	std::string ToJson(const CharacterCustomizationComponent& component, const Skeleton* skeleton = nullptr);

	// Parses JSON text into the component. Unknown bones are kept so the
	// preset round-trips even when applied to a different skeleton. Returns
	// false when the document is malformed.
	bool FromJson(const std::string& json, CharacterCustomizationComponent& outComponent);

	bool Save(const CharacterCustomizationComponent& component, const std::string& path, const Skeleton* skeleton = nullptr);
	bool Load(const std::string& path, CharacterCustomizationComponent& outComponent);

	// --- Preset library (browser) helpers ---
	constexpr const char* kDefaultPresetDirectory = "assets/characters";

	// Lists *.character.json files in a directory (non-recursive), sorted by
	// filename. Paths use forward slashes. Missing directory => empty list.
	std::vector<std::string> ListPresets(const std::string& directory = kDefaultPresetDirectory);

	// "assets/characters/foo.character.json" => "foo".
	std::string PresetDisplayName(const std::string& path);

	// directory + name + extension; appends the extension when missing.
	std::string MakePresetPath(const std::string& directory, const std::string& name);

	// Copies a preset to a new file next to it. If newName is empty a unique
	// " Copy"/" Copy N" suffix is generated. Returns the new path or empty.
	std::string Duplicate(const std::string& sourcePath, const std::string& newName = {});

	// Renames a preset (same directory). Returns the new path or empty.
	std::string Rename(const std::string& sourcePath, const std::string& newName);

	bool Delete(const std::string& path);
}
