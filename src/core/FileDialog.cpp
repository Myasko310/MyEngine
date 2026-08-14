#include "core/FileDialog.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace MyEngine
{
	namespace FileDialog
	{
#ifdef _WIN32
		std::string OpenSceneFile()
		{
			char fileName[MAX_PATH] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::string SaveSceneFile()
		{
			char fileName[MAX_PATH] = "scene.json";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetSaveFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::string OpenImageFile()
		{
			char fileName[MAX_PATH] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::string OpenScriptFile()
		{
			char fileName[MAX_PATH] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Lua Scripts (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}
#else
		std::string OpenSceneFile()
		{
			return std::string();
		}

		std::string SaveSceneFile()
		{
			return std::string();
		}

		std::string OpenImageFile()
		{
			return std::string();
		}

		std::string OpenScriptFile()
		{
			return std::string();
		}
#endif
	}
}
