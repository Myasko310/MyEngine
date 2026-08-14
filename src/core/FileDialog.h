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
	}
}
