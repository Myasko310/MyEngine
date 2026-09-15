#include "AssetBrowserPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>

#include "animation/AnimationStateMachine.h"
#include "components/AnimationComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/ScriptComponent.h"
#include "components/TransformComponent.h"
#include "core/AssetManager.h"
#include "core/AssetPipeline.h"
#include "editor/EditorStyle.h"
#include "rendering/Skeleton.h"
#include "serialization/SceneSerializer.h"

namespace MyEngine::Editor::Panels
{
	void DrawAssetBrowserPanel(Context& context)
	{
		if (!context.ui || !context.scene || !context.selectedEntity || !context.undoStack)
			return;

		auto& ui = *context.ui;
		auto& scene = *context.scene;
		auto& selectedEntity = *context.selectedEntity;

		if (!ui.showAssetBrowser)
			return;

		ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
		ImGui::Begin("Asset Browser", &ui.showAssetBrowser);

		auto& pipeline = MyEngine::AssetPipeline::Get();
		pipeline.Tick();

		namespace fs = std::filesystem;
		static char assetFilter[64] = "";
		static std::string selectedAssetPath;
		auto matchesFilter = [&](const std::string& value)
		{
			if (assetFilter[0] == '\0')
				return true;
			std::string lhs = value;
			std::string rhs = assetFilter;
			std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lhs.find(rhs) != std::string::npos;
		};

		ImGui::TextDisabled("Browse project assets and double-click to open or apply supported files.");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##assetBrowserFilter", "Filter assets or folders", assetFilter, sizeof(assetFilter));
		if (InspectorActionButton("Scan Asset Metadata##assetPipelineScan"))
			pipeline.ScanAssets("assets");
		if (InspectorActionButton("Queue Folder Imports##assetPipelineQueueFolder") &&
			fs::exists(ui.assetBrowserPath) && fs::is_directory(ui.assetBrowserPath))
		{
			for (const auto& entry : fs::directory_iterator(ui.assetBrowserPath))
			{
				if (entry.is_regular_file())
					pipeline.QueueImport(entry.path().generic_string());
			}
		}

		ImGui::TextDisabled("Jobs - Pending: %d  Completed: %d  Failed: %d  Duplicates: %d",
			pipeline.GetPendingJobCount(),
			pipeline.GetCompletedJobCount(),
			pipeline.GetFailedJobCount(),
			pipeline.GetDeduplicatedJobCount());
		if (InspectorActionButton("Cancel Queued Imports##assetPipelineCancel"))
			pipeline.ClearQueuedImports();

		if (ImGui::CollapsingHeader("Asset Pipeline##assetPipelineStatus"))
		{
			auto metadata = pipeline.GetMetadataSnapshot();
			ImGui::TextDisabled("Indexed Assets: %d", static_cast<int>(metadata.size()));
			if (!metadata.empty())
			{
				int modelCount = 0;
				int textureCount = 0;
				int audioCount = 0;
				for (const auto& m : metadata)
				{
					if (m.type == "model") ++modelCount;
					else if (m.type == "texture") ++textureCount;
					else if (m.type == "audio") ++audioCount;
				}
				ImGui::TextDisabled("Models: %d  Textures: %d  Audio: %d", modelCount, textureCount, audioCount);
			}

			if (!selectedAssetPath.empty())
			{
				ImGui::Separator();
				ImGui::TextWrapped("Selected Asset: %s", selectedAssetPath.c_str());
				if (InspectorActionButton("Queue Reimport Selected##assetPipelineReimportSelected"))
					pipeline.QueueImport(selectedAssetPath);
				if (InspectorActionButton("Queue Reimport Dependents##assetPipelineReimportDependents"))
					pipeline.QueueReimportDependents(selectedAssetPath);

				auto selectedMeta = std::find_if(metadata.begin(), metadata.end(),
					[&](const MyEngine::AssetMetadata& m) { return m.path == selectedAssetPath; });
				if (selectedMeta != metadata.end())
				{
					ImGui::TextDisabled("Type: %s  Dependencies: %d", selectedMeta->type.c_str(), static_cast<int>(selectedMeta->dependencies.size()));
					for (const auto& dep : selectedMeta->dependencies)
						ImGui::BulletText("dep: %s", dep.c_str());
				}

				auto dependents = pipeline.GetDependentsForAsset(selectedAssetPath);
				if (!dependents.empty())
				{
					ImGui::TextDisabled("Dependents: %d", static_cast<int>(dependents.size()));
					for (const auto& dep : dependents)
						ImGui::BulletText("uses: %s", dep.c_str());
				}
			}

			auto events = pipeline.GetRecentEvents();
			if (!events.empty())
			{
				ImGui::Separator();
				ImGui::TextDisabled("Recent Jobs:");
				for (const auto& evt : events)
					ImGui::TextWrapped("- %s", evt.c_str());
			}
		}

		if (ui.assetBrowserPath != "assets")
		{
			if (InspectorActionButton("Back to Parent Folder##assetBrowserBack"))
			{
				fs::path parent = fs::path(ui.assetBrowserPath).parent_path();
				ui.assetBrowserPath = parent.empty() ? "assets" : parent.generic_string();
			}
		}
		ImGui::TextWrapped("Path: %s", ui.assetBrowserPath.c_str());
		InspectorGroupLabel("Contents");

		if (fs::exists(ui.assetBrowserPath) && fs::is_directory(ui.assetBrowserPath))
		{
			ImGui::BeginChild("##assetBrowserList", ImVec2(0, 0), true);
			for (const auto& entry : fs::directory_iterator(ui.assetBrowserPath))
			{
				if (!entry.is_directory())
					continue;
				std::string folderName = entry.path().filename().string();
				if (!matchesFilter(folderName))
					continue;
				std::string label = "[Folder] " + folderName;
				if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
					ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					ui.assetBrowserPath = entry.path().generic_string();
				}
			}

			for (const auto& entry : fs::directory_iterator(ui.assetBrowserPath))
			{
				if (!entry.is_regular_file())
					continue;

				std::string filePath = entry.path().generic_string();
				std::string fileName = entry.path().filename().string();
				if (!matchesFilter(fileName))
					continue;
				std::string ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

				std::string labelPrefix = "[File] ";
				if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")
					labelPrefix = "[Model] ";
				else if (ext == ".wav")
					labelPrefix = "[Audio] ";
				else if (filePath.find(".material.json") != std::string::npos)
					labelPrefix = "[Material] ";
				else if (filePath.find(".animstate.json") != std::string::npos)
					labelPrefix = "[Anim SM] ";
				else if (ext == ".scene" || ext == ".json")
					labelPrefix = "[Scene] ";

				const bool isSelectedAsset = (selectedAssetPath == filePath);
				if (ImGui::Selectable((labelPrefix + fileName).c_str(), isSelectedAsset, ImGuiSelectableFlags_AllowDoubleClick))
					selectedAssetPath = filePath;
				const bool doubleClicked = ImGui::IsItemHovered() &&
					ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

				if (doubleClicked)
				{
					pipeline.QueueImport(filePath);
					const bool isStaticModel = (ext == ".obj");
					const bool isSkinnedCandidate = (ext == ".gltf" || ext == ".glb" || ext == ".fbx");

					// Helper to resolve the actual model path if it's in a nested structure
					auto resolveModelPath = [](const std::string& originalPath) -> std::string
					{
						// For FBX files, check if there's a "source" subfolder variant
						if (originalPath.find(".fbx") != std::string::npos)
						{
							namespace fs = std::filesystem;
							fs::path original(originalPath);

							// Check for adjacent "source" subfolder
							fs::path sourceVariant = original.parent_path() / "source" / original.filename();
							if (fs::exists(sourceVariant))
							{
								std::cout << "[AssetBrowser] Using source variant: " << sourceVariant.generic_string() << std::endl;
								return sourceVariant.generic_string();
							}
						}
						return originalPath;
					};

					std::string modelPath = resolveModelPath(filePath);

					if (isStaticModel || isSkinnedCandidate)
					{
						const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
						auto ent = scene.CreateEntity(entry.path().stem().string());
						auto& t = ent->AddComponent<TransformComponent>();
						t.position = glm::vec3(0.0f, 0.5f, -3.0f);

						const bool isRinAnimatedModel = entry.path().filename().string() == "rin_tohsaka_anim.glb";
						bool attached = false;

						// Try loading as skinned model first (may have skeleton or not)
						if (isSkinnedCandidate && context.litSkinnedShader)
						{
							MyEngine::SkinnedModelData skinnedData = MyEngine::AssetManager::LoadSkinnedModel(modelPath);
							if (!skinnedData.meshes.empty())
							{
								if (skinnedData.skeleton && skinnedData.skeleton->GetBoneCount() > 0)
								{
									// Has skeleton - attach as skinned
									MyEngine::AssetManager::AttachSkinnedModelToEntity(ent, skinnedData, context.litSkinnedShader, modelPath);
									if (isRinAnimatedModel)
									{
										auto& script = ent->HasComponent<ScriptComponent>()
											? ent->GetComponent<ScriptComponent>()
											: ent->AddComponent<ScriptComponent>();
										script.scriptPath = "assets/scripts/rin_animation_hotkeys.lua";
										script.requestReload = true;
									}
									attached = true;
								}
								else
								{
									// No skeleton but has meshes - attach as static
									MyEngine::AssetManager::AttachMeshToEntity(ent, skinnedData.meshes[0], modelPath, context.litShader);
									attached = true;
								}
							}
						}

						// Fallback if skinned loading failed or file is OBJ
						if (!attached && context.litShader)
						{
							auto meshes = MyEngine::AssetManager::LoadModel(modelPath);
							if (!meshes.empty())
							{
								MyEngine::AssetManager::AttachMeshToEntity(ent, meshes[0], modelPath, context.litShader);
								attached = true;
							}
						}

						// If mesh is attached, try to import and apply materials
						if (attached)
						{
							auto importedMats = MyEngine::AssetManager::ImportModelMaterials(modelPath);

							auto& mr = ent->GetComponent<MeshRendererComponent>();

							if (!importedMats.empty() && importedMats[0])
							{
								mr.material = importedMats[0];
								mr.materialPath = importedMats[0]->GetPath();

								// Use PBR shader if material has PBR properties, otherwise use material's shader
								if (importedMats[0]->shader)
								{
									mr.shader = importedMats[0]->shader;
									std::cout << "[AssetBrowser] Applied material shader from asset: " << mr.materialPath << std::endl;
								}
								else if (context.pbrShader && 
										(importedMats[0]->usePBR || 
										 importedMats[0]->metallicRoughnessMap != nullptr ||
										 (importedMats[0]->metallic > 0.0f || importedMats[0]->roughness < 1.0f)))
								{
									// Material has PBR workflow properties, use PBR shader
									mr.shader = context.pbrShader;
									mr.usePBR = true;
									std::cout << "[AssetBrowser] Applied PBR shader to entity: " << ent->GetName() << std::endl;
								}
								else if (!ent->HasComponent<AnimationComponent>() && context.litShader)
								{
									// Fallback to lit shader for non-animated models without PBR properties
									mr.shader = context.litShader;
									std::cout << "[AssetBrowser] Applied lit shader to entity: " << ent->GetName() << std::endl;
								}
							}
							else
							{
								// No materials imported, ensure we have a basic shader
								if (!mr.shader && context.litShader)
								{
									mr.shader = context.litShader;
									// Set a default visible color if no material
									mr.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
									std::cout << "[AssetBrowser] No materials found, using default lit shader with grey albedo" << std::endl;
								}
							}

							selectedEntity = ent.get();
							const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
							if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
								context.pushSceneStateCommand(beforeState, afterState);
						}
						else
						{
							std::cout << "[AssetBrowser] Failed to load model: " << modelPath << std::endl;
							scene.DestroyEntity(ent->GetID());
						}
					}
					else if (ext == ".wav")
					{
						if (selectedEntity)
						{
							const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
							if (!selectedEntity->HasComponent<AudioSourceComponent>())
								selectedEntity->AddComponent<AudioSourceComponent>();
							auto& as = selectedEntity->GetComponent<AudioSourceComponent>();
							as.clipPath = filePath;
							as.clip = MyEngine::AssetManager::LoadAudioClip(filePath);
							const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
							if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
								context.pushSceneStateCommand(beforeState, afterState);
						}
					}
					else if (filePath.find(".material.json") != std::string::npos)
					{
						ui.showMaterialBrowser = true;
						ui.selectedMaterialPath = filePath;
						ui.editingMaterial = MyEngine::AssetManager::LoadMaterial(filePath);
						ui.matBrowserTexturesScanned = false;
					}
					else if (filePath.find(".animstate.json") != std::string::npos)
					{
						ui.showInspector = true;
						ui.selectedAnimationStateMachinePath = filePath;
						ui.editingAnimationStateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
						if (!ui.editingAnimationStateMachine->LoadFromFile(filePath))
							ui.editingAnimationStateMachine.reset();
					}
					else if (ext == ".scene" || ext == ".json")
					{
						const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
						selectedEntity = nullptr;
						context.undoStack->Clear();
						if (MyEngine::Serialization::LoadScene(scene, filePath, context.litShader, &ui.globalScripts))
						{
							if (context.currentScenePath)
								*context.currentScenePath = filePath;
							if (context.recentScenes && context.addRecentScene && context.currentScenePath)
								context.addRecentScene(*context.recentScenes, *context.currentScenePath);
							if (context.window && context.updateWindowTitle && context.currentScenePath)
								context.updateWindowTitle(context.window, *context.currentScenePath);
							const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
							if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
								context.pushSceneStateCommand(beforeState, afterState);
						}
					}
				}

				if (ImGui::IsItemHovered())
				{
					if (ext == ".obj")
						ImGui::SetTooltip("Double-click: add model to scene");
					else if (ext == ".wav")
						ImGui::SetTooltip("Double-click: assign clip to selected entity's audio source");
					else if (filePath.find(".material.json") != std::string::npos)
						ImGui::SetTooltip("Double-click: open in Material Browser");
					else if (filePath.find(".animstate.json") != std::string::npos)
						ImGui::SetTooltip("Double-click: open animation state machine asset");
					else if (ext == ".scene" || ext == ".json")
						ImGui::SetTooltip("Double-click: open scene");
				}
			}
			ImGui::EndChild();
		}
		else
		{
			ImGui::TextDisabled("Folder not found: %s", ui.assetBrowserPath.c_str());
		}

		ImGui::End();
	}
}
#endif
