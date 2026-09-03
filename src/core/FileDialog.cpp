#include "core/FileDialog.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include <vector>

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

		std::string OpenMaterialFile()
		{
			char fileName[MAX_PATH] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Material Files (*.material.json)\0*.material.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::string SaveMaterialFile()
		{
			char fileName[MAX_PATH] = "new_material.material.json";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Material Files (*.material.json)\0*.material.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetSaveFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::string OpenModelFile()
		{
			char fileName[MAX_PATH] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "3D Model Files (*.obj;*.fbx;*.gltf;*.glb;*.dae)\0*.obj;*.fbx;*.gltf;*.glb;*.dae\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);

			return std::string();
		}

		std::vector<std::string> OpenModelFiles()
		{
			char buffer[32768] = "";

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "3D Model Files (*.obj;*.fbx;*.gltf;*.glb;*.dae)\0*.obj;*.fbx;*.gltf;*.glb;*.dae\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = buffer;
			ofn.nMaxFile = sizeof(buffer);
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

			if (!GetOpenFileNameA(&ofn))
				return {};

			std::vector<std::string> selectedFiles;
			const char* cursor = buffer;
			std::string first = cursor;
			if (first.empty())
				return selectedFiles;

			cursor += first.size() + 1;
			if (*cursor == '\0')
			{
				selectedFiles.push_back(first);
				return selectedFiles;
			}

			std::string directory = first;
			while (*cursor != '\0')
			{
				std::string fileName = cursor;
				selectedFiles.push_back(directory + "\\" + fileName);
				cursor += fileName.size() + 1;
			}

			return selectedFiles;
		}

		std::string OpenPrefabFile()
		{
			char fileName[MAX_PATH] = "";
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Prefab Files (*.prefab.json)\0*.prefab.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);
			return std::string();
		}

		std::string SavePrefabFile()
		{
			char fileName[MAX_PATH] = "new_prefab.prefab.json";
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Prefab Files (*.prefab.json)\0*.prefab.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (GetSaveFileNameA(&ofn))
				return std::string(fileName);
			return std::string();
		}

		std::string OpenAnimationStateMachineFile()
		{
			char fileName[MAX_PATH] = "";
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Animation State Machine Files (*.animstate.json)\0*.animstate.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (GetOpenFileNameA(&ofn))
				return std::string(fileName);
			return std::string();
		}

		std::string SaveAnimationStateMachineFile()
		{
			char fileName[MAX_PATH] = "new_state_machine.animstate.json";
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = "Animation State Machine Files (*.animstate.json)\0*.animstate.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "json";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (GetSaveFileNameA(&ofn))
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

		std::string OpenMaterialFile()
		{
			return std::string();
		}

		std::string SaveMaterialFile()
		{
			return std::string();
		}

		std::string OpenModelFile()
		{
			return std::string();
		}

		std::vector<std::string> OpenModelFiles()
		{
			return {};
		}

		std::string OpenPrefabFile()
		{
			return std::string();
		}

		std::string SavePrefabFile()
		{
			return std::string();
		}

		std::string OpenAnimationStateMachineFile()
		{
			return std::string();
		}

		std::string SaveAnimationStateMachineFile()
		{
			return std::string();
		}
#endif
	}
}
