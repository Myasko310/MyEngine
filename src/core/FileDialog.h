#pragma once

#include <string>

namespace MyEngine
{
	namespace FileDialog
	{
		// Opens a native "Open File" dialog filtered to *.json scene files.
		// Returns the selected file path, or an empty string if cancelled.
		std::string OpenSceneFile();

		// Opens a native "Save File" dialog filtered to *.json scene files.
		// Returns the selected file path (with .json extension ensured), or an empty string if cancelled.
		std::string SaveSceneFile();

		// Opens a native "Open File" dialog filtered to common image formats
		// (used for skybox faces, textures, etc). Returns the selected file
		// path, or an empty string if cancelled.
		std::string OpenImageFile();

		// Opens a native "Open File" dialog filtered to Lua scripts.
		// Returns the selected file path, or an empty string if cancelled.
		std::string OpenScriptFile();

		// Opens a native "Open File" dialog filtered to *.material.json files.
		// Returns the selected file path, or an empty string if cancelled.
		std::string OpenMaterialFile();

		// Opens a native "Save File" dialog filtered to *.material.json files.
		// Returns the selected file path (with .material.json extension ensured),
		// or an empty string if cancelled.
		std::string SaveMaterialFile();

		// Opens a native "Open File" dialog filtered to common 3D model formats.
		// Returns the selected file path, or an empty string if cancelled.
		std::string OpenModelFile();

		// Opens a native "Open File" dialog filtered to *.prefab.json files.
		std::string OpenPrefabFile();

		// Opens a native "Save File" dialog filtered to *.prefab.json files.
		std::string SavePrefabFile();

		// Opens a native "Open File" dialog filtered to *.animstate.json files.
		std::string OpenAnimationStateMachineFile();

		// Opens a native "Save File" dialog filtered to *.animstate.json files.
		std::string SaveAnimationStateMachineFile();
	}
}
