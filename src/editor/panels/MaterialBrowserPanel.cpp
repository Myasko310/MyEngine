#include "MaterialBrowserPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

#include "components/AnimationComponent.h"
#include "components/MeshRendererComponent.h"
#include "core/AssetManager.h"
#include "core/FileDialog.h"
#include "editor/EditorStyle.h"
#include "editor/EditorUndo.h"
#include "rendering/Material.h"
#include "rendering/Texture.h"

namespace MyEngine::Editor::Panels
{
	void DrawMaterialBrowserPanel(Context& context)
	{
		if (!context.ui || !context.selectedEntity)
			return;

		auto& ui = *context.ui;
		auto* selectedEntity = *context.selectedEntity;
		if (!ui.showMaterialBrowser)
			return;

		ImGui::SetNextWindowPos(ImVec2(320, 440), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(680, 560), ImGuiCond_FirstUseEver);
		ImGui::Begin("Material Browser", &ui.showMaterialBrowser);

		namespace fs = std::filesystem;
		static char materialFilter[64] = "";
		auto matchesFilter = [&](const std::string& value)
		{
			if (materialFilter[0] == '\0')
				return true;
			std::string lhs = value;
			std::string rhs = materialFilter;
			std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lhs.find(rhs) != std::string::npos;
		};
		{
			std::error_code ec;
			fs::create_directories(ui.materialBrowserPath, ec);
		}

		ImGui::TextDisabled("Browse, create, edit, and assign material assets.");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##materialFilter", "Filter materials or folders", materialFilter, sizeof(materialFilter));
		if (ImGui::Button("New Material", ImVec2(120.0f, 0.0f)))
			ui.showNewMaterialDialog = true;

		ImGui::SameLine();
		if (ImGui::Button("Open...", ImVec2(90.0f, 0.0f)))
		{
			std::string picked = MyEngine::FileDialog::OpenMaterialFile();
			if (!picked.empty())
			{
				ui.selectedMaterialPath = picked;
				ui.editingMaterial = MyEngine::AssetManager::LoadMaterial(picked);
				ui.matBrowserTexturesScanned = false;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Refresh", ImVec2(90.0f, 0.0f)))
			ui.matBrowserTexturesScanned = false;

		ImGui::TextWrapped("Directory: %s", ui.materialBrowserPath.c_str());
		InspectorGroupLabel("Material Assets");

		if (ui.showNewMaterialDialog)
		{
			InspectorGroupLabel("Create New Material");
			InspectorFullWidth();
			ImGui::InputText("Name##newmat", ui.newMaterialNameBuffer, sizeof(ui.newMaterialNameBuffer));
			if (InspectorActionButton("Create##newmat"))
			{
				std::string newPath = ui.materialBrowserPath + "/" + ui.newMaterialNameBuffer;
				if (newPath.find(".material.json") == std::string::npos)
					newPath += ".material.json";

				auto mat = std::make_shared<MyEngine::Material>();
				mat->SetPath(newPath);
				mat->shaderVertexPath = "shaders/lit.vert";
				mat->shaderFragmentPath = "shaders/lit.frag";
				mat->shader = MyEngine::AssetManager::LoadShader(mat->shaderVertexPath, mat->shaderFragmentPath);
				mat->SaveToFile(newPath);

				ui.selectedMaterialPath = newPath;
				ui.editingMaterial = mat;
				ui.showNewMaterialDialog = false;
			}
			if (InspectorDangerButton("Cancel##newmat"))
				ui.showNewMaterialDialog = false;
		}

		float listWidth = 240.0f;
		ImGui::BeginChild("##matlist", ImVec2(listWidth, 0), true);

		if (ui.materialBrowserPath != "assets/materials")
		{
			if (InspectorActionButton("Back to Parent Folder##materialBrowserBack"))
			{
				fs::path parent = fs::path(ui.materialBrowserPath).parent_path();
				ui.materialBrowserPath = parent.empty() ? "assets/materials" : parent.generic_string();
				ui.selectedMaterialPath.clear();
				ui.editingMaterial = nullptr;
			}
		}
		ImGui::TextWrapped("Folder: %s", fs::path(ui.materialBrowserPath).filename().string().c_str());
		ImGui::Separator();

		if (fs::exists(ui.materialBrowserPath) && fs::is_directory(ui.materialBrowserPath))
		{
			for (const auto& entry : fs::directory_iterator(ui.materialBrowserPath))
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
					ui.materialBrowserPath = entry.path().generic_string();
					ui.selectedMaterialPath.clear();
					ui.editingMaterial = nullptr;
				}
			}

			for (const auto& entry : fs::directory_iterator(ui.materialBrowserPath))
			{
				if (!entry.is_regular_file())
					continue;
				std::string filePath = entry.path().generic_string();
				std::string fileName = entry.path().filename().string();
				if (!matchesFilter(fileName) || filePath.find(".json") == std::string::npos)
					continue;

				bool isSelected = (filePath == ui.selectedMaterialPath);
				if (ImGui::Selectable((std::string("[Material] ") + fileName).c_str(), isSelected))
				{
					ui.selectedMaterialPath = filePath;
					ui.editingMaterial = MyEngine::AssetManager::LoadMaterial(filePath);
					ui.matBrowserTexturesScanned = false;
					ui.materialRenameActive = false;
				}

				if (ImGui::BeginPopupContextItem(filePath.c_str()))
				{
					if (ImGui::MenuItem("Edit"))
					{
						ui.selectedMaterialPath = filePath;
						ui.editingMaterial = MyEngine::AssetManager::LoadMaterial(filePath);
						ui.matBrowserTexturesScanned = false;
					}
					if (ImGui::MenuItem("Assign to Selected Entity"))
					{
						if (selectedEntity && selectedEntity->HasComponent<MeshRendererComponent>())
						{
							auto mat = MyEngine::AssetManager::LoadMaterial(filePath);
							if (mat)
							{
								auto& mr = selectedEntity->GetComponent<MeshRendererComponent>();
								mr.material = mat;
								mr.materialPath = filePath;
								if (mat->shader)
									mr.shader = mat->shader;
							}
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Rename"))
					{
						ui.selectedMaterialPath = filePath;
						std::strncpy(ui.materialRenameBuffer, fileName.c_str(), sizeof(ui.materialRenameBuffer) - 1);
						ui.materialRenameBuffer[sizeof(ui.materialRenameBuffer) - 1] = '\0';
						ui.materialRenameActive = true;
					}
					if (ImGui::MenuItem("Delete"))
					{
						std::error_code ec;
						fs::remove(filePath, ec);
						if (ui.selectedMaterialPath == filePath)
						{
							ui.selectedMaterialPath.clear();
							ui.editingMaterial = nullptr;
						}
					}
					ImGui::EndPopup();
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Click: select | Right-click: options");
			}
		}
		else
		{
			ImGui::TextDisabled("Folder not found");
		}

		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::SameLine();
		ImGui::BeginChild("##mateditor", ImVec2(0, 0), false);

		if (!ui.selectedMaterialPath.empty() && ui.editingMaterial)
		{
			auto& mat = *ui.editingMaterial;
			auto materialSnapshotsEqual = [](const EditorUndo::MaterialSnapshot& lhs, const EditorUndo::MaterialSnapshot& rhs)
			{
				return lhs.path == rhs.path &&
					lhs.shaderVertexPath == rhs.shaderVertexPath &&
					lhs.shaderFragmentPath == rhs.shaderFragmentPath &&
					lhs.albedo == rhs.albedo &&
					lhs.shininess == rhs.shininess &&
					lhs.useTexture == rhs.useTexture &&
					lhs.usePBR == rhs.usePBR &&
					lhs.metallic == rhs.metallic &&
					lhs.roughness == rhs.roughness &&
					lhs.aoStrength == rhs.aoStrength &&
					lhs.emissive == rhs.emissive &&
					lhs.blendMode == rhs.blendMode &&
					lhs.cullMode == rhs.cullMode &&
					lhs.depthWrite == rhs.depthWrite &&
					lhs.depthTest == rhs.depthTest &&
					lhs.renderQueue == rhs.renderQueue &&
					lhs.texturePath == rhs.texturePath &&
					lhs.albedoMapPath == rhs.albedoMapPath &&
					lhs.normalMapPath == rhs.normalMapPath &&
					lhs.metallicRoughnessMapPath == rhs.metallicRoughnessMapPath &&
					lhs.aoMapPath == rhs.aoMapPath &&
					lhs.emissiveMapPath == rhs.emissiveMapPath;
			};
			auto pushMaterialUndo = [&](const EditorUndo::MaterialSnapshot& beforeSnapshot)
			{
				if (!context.undoStack || !ui.editingMaterial)
					return;
				EditorUndo::MaterialSnapshot afterSnapshot = EditorUndo::MaterialEditCommand::Capture(*ui.editingMaterial);
				if (materialSnapshotsEqual(beforeSnapshot, afterSnapshot))
					return;
				context.undoStack->Push(std::make_unique<EditorUndo::MaterialEditCommand>(
					ui.editingMaterial,
					beforeSnapshot,
					afterSnapshot));
			};
			ImGui::TextWrapped("Selected Material: %s", fs::path(ui.selectedMaterialPath).filename().string().c_str());
			ImGui::TextDisabled("Full Path: %s", ui.selectedMaterialPath.c_str());
			ImGui::Separator();

			if (ui.materialRenameActive)
			{
				ImGui::SetNextItemWidth(300.0f);
				if (ImGui::InputText("##rename", ui.materialRenameBuffer, sizeof(ui.materialRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					std::string newPath = fs::path(ui.selectedMaterialPath).parent_path().generic_string() + "/" + ui.materialRenameBuffer;
					std::error_code ec;
					fs::rename(ui.selectedMaterialPath, newPath, ec);
					if (!ec)
					{
						mat.SetPath(newPath);
						ui.selectedMaterialPath = newPath;
					}
					ui.materialRenameActive = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("OK##ren"))
				{
					std::string newPath = fs::path(ui.selectedMaterialPath).parent_path().generic_string() + "/" + ui.materialRenameBuffer;
					std::error_code ec;
					fs::rename(ui.selectedMaterialPath, newPath, ec);
					if (!ec)
					{
						mat.SetPath(newPath);
						ui.selectedMaterialPath = newPath;
					}
					ui.materialRenameActive = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel##ren"))
					ui.materialRenameActive = false;
			}
			else
			{
				ImGui::TextDisabled("%s", ui.selectedMaterialPath.c_str());
			}

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Shader", ImGuiTreeNodeFlags_DefaultOpen))
			{
				static char vertBuf[256];
				static char fragBuf[256];
				static std::string lastMatPath;
				if (lastMatPath != ui.selectedMaterialPath)
				{
					lastMatPath = ui.selectedMaterialPath;
					std::strncpy(vertBuf, mat.shaderVertexPath.c_str(), sizeof(vertBuf) - 1);
					std::strncpy(fragBuf, mat.shaderFragmentPath.c_str(), sizeof(fragBuf) - 1);
					vertBuf[sizeof(vertBuf) - 1] = '\0';
					fragBuf[sizeof(fragBuf) - 1] = '\0';
				}
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("Vertex##sv", vertBuf, sizeof(vertBuf));
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("Fragment##sf", fragBuf, sizeof(fragBuf));
				if (ImGui::Button("Apply Shader"))
				{
					EditorUndo::MaterialSnapshot beforeSnapshot = EditorUndo::MaterialEditCommand::Capture(mat);
					mat.shaderVertexPath = vertBuf;
					mat.shaderFragmentPath = fragBuf;
					try { mat.shader = MyEngine::AssetManager::LoadShader(vertBuf, fragBuf); }
					catch (...) { mat.shader = nullptr; }
					pushMaterialUndo(beforeSnapshot);
				}
				ImGui::SameLine();
				if (ImGui::Button("Lit"))
				{
					std::strncpy(vertBuf, "shaders/lit.vert", sizeof(vertBuf) - 1);
					std::strncpy(fragBuf, "shaders/lit.frag", sizeof(fragBuf) - 1);
					vertBuf[sizeof(vertBuf) - 1] = '\0';
					fragBuf[sizeof(fragBuf) - 1] = '\0';
				}
				ImGui::SameLine();
				if (ImGui::Button("PBR"))
				{
					std::strncpy(vertBuf, "shaders/pbr.vert", sizeof(vertBuf) - 1);
					std::strncpy(fragBuf, "shaders/pbr.frag", sizeof(fragBuf) - 1);
					vertBuf[sizeof(vertBuf) - 1] = '\0';
					fragBuf[sizeof(fragBuf) - 1] = '\0';
				}
				ImGui::TextDisabled("Loaded: %s", mat.shader ? "yes" : "no");
			}

			if (ImGui::CollapsingHeader("Surface", ImGuiTreeNodeFlags_DefaultOpen))
			{
				EditorUndo::MaterialSnapshot beforeSurfaceSnapshot = EditorUndo::MaterialEditCommand::Capture(mat);
				bool surfaceEdited = false;
				surfaceEdited |= ImGui::ColorEdit3("Albedo", &mat.albedo.x);
				surfaceEdited |= ImGui::DragFloat("Shininess", &mat.shininess, 1.0f, 0.0f, 256.0f);
				surfaceEdited |= ImGui::Checkbox("Use Texture", &mat.useTexture);

				if (!ui.matBrowserTexturesScanned)
				{
					ui.matBrowserTextures.clear();
					std::error_code ec2;
					if (fs::exists("assets/textures", ec2))
					{
						for (const auto& te : fs::recursive_directory_iterator("assets/textures", ec2))
						{
							if (!te.is_regular_file())
								continue;
							std::string ext2 = te.path().extension().string();
							std::transform(ext2.begin(), ext2.end(), ext2.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
							if (ext2 == ".png" || ext2 == ".jpg" || ext2 == ".jpeg" || ext2 == ".bmp" || ext2 == ".tga")
								ui.matBrowserTextures.push_back(te.path().generic_string());
						}
						std::sort(ui.matBrowserTextures.begin(), ui.matBrowserTextures.end());
					}
					ui.matBrowserTexturesScanned = true;
				}

				auto texPicker = [&](const char* label, std::shared_ptr<MyEngine::Texture>& slot)
				{
					bool changed = false;
					std::string cur = slot ? slot->GetPath() : "None";
					if (ImGui::BeginCombo(label, cur.c_str()))
					{
						if (ImGui::Selectable("None", !slot))
						{
							slot = nullptr;
							changed = true;
						}
						for (const auto& tp : ui.matBrowserTextures)
						{
							bool sel = slot && slot->GetPath() == tp;
							if (ImGui::Selectable(tp.c_str(), sel))
							{
								try
								{
									slot = MyEngine::AssetManager::LoadTexture(tp);
									changed = true;
								}
								catch (...) {}
							}
							if (sel)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					return changed;
				};

				surfaceEdited |= texPicker("Texture##matbase", mat.texture);
				ImGui::SameLine();
				if (ImGui::Button("Browse##basetex"))
				{
					std::string p = MyEngine::FileDialog::OpenImageFile();
					if (!p.empty())
					{
						try { mat.texture = MyEngine::AssetManager::LoadTexture(p); mat.useTexture = true; surfaceEdited = true; }
						catch (...) {}
					}
				}
				if (surfaceEdited && ImGui::IsItemDeactivatedAfterEdit())
					pushMaterialUndo(beforeSurfaceSnapshot);
			}

			if (ImGui::CollapsingHeader("PBR##matHeader"))
			{
				EditorUndo::MaterialSnapshot beforePbrSnapshot = EditorUndo::MaterialEditCommand::Capture(mat);
				bool pbrEdited = false;
				pbrEdited |= ImGui::Checkbox("Use PBR##matpbr", &mat.usePBR);
				if (mat.usePBR)
				{
					pbrEdited |= ImGui::SliderFloat("Metallic##matMetallic", &mat.metallic, 0.0f, 1.0f);
					pbrEdited |= ImGui::SliderFloat("Roughness##matRoughness", &mat.roughness, 0.04f, 1.0f);
					pbrEdited |= ImGui::SliderFloat("AO##matAOStrength", &mat.aoStrength, 0.0f, 1.0f);
					pbrEdited |= ImGui::ColorEdit3("Emissive##matEmissiveColor", &mat.emissive.x);
					ImGui::Separator();
					auto texPicker2 = [&](const char* label, std::shared_ptr<MyEngine::Texture>& slot)
					{
						bool changed = false;
						std::string cur = slot ? slot->GetPath() : "None";
						if (ImGui::BeginCombo(label, cur.c_str()))
						{
							if (ImGui::Selectable("None", !slot))
							{
								slot = nullptr;
								changed = true;
							}
							for (const auto& tp : ui.matBrowserTextures)
							{
								bool sel = slot && slot->GetPath() == tp;
								if (ImGui::Selectable(tp.c_str(), sel))
								{
									try
									{
										slot = MyEngine::AssetManager::LoadTexture(tp);
										changed = true;
									}
									catch (...) {}
								}
								if (sel)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::SameLine();
						std::string browseId = std::string("Browse##") + label;
						if (ImGui::Button(browseId.c_str()))
						{
							std::string p = MyEngine::FileDialog::OpenImageFile();
							if (!p.empty())
							{
								try
								{
									slot = MyEngine::AssetManager::LoadTexture(p);
									changed = true;
								}
								catch (...) {}
							}
						}
						return changed;
					};
					pbrEdited |= texPicker2("Albedo Map##matAlbedo", mat.albedoMap);
					pbrEdited |= texPicker2("Normal Map##matNormal", mat.normalMap);
					pbrEdited |= texPicker2("MetallicRoughness##matMetalRough", mat.metallicRoughnessMap);
					pbrEdited |= texPicker2("AO Map##matAO", mat.aoMap);
					pbrEdited |= texPicker2("Emissive Map##matEmissive", mat.emissiveMap);
				}
				if (pbrEdited && ImGui::IsItemDeactivatedAfterEdit())
					pushMaterialUndo(beforePbrSnapshot);
			}

			if (ImGui::CollapsingHeader("Render Flags##matRenderFlags"))
			{
				EditorUndo::MaterialSnapshot beforeRenderSnapshot = EditorUndo::MaterialEditCommand::Capture(mat);
				bool renderEdited = false;
				const char* blendItems[] = { "Opaque", "Alpha Blend", "Additive" };
				int blendIdx = static_cast<int>(mat.blendMode);
				if (ImGui::Combo("Blend Mode##matBlend", &blendIdx, blendItems, IM_ARRAYSIZE(blendItems)))
				{
					mat.blendMode = static_cast<MyEngine::BlendMode>(blendIdx);
					renderEdited = true;
				}

				const char* cullItems[] = { "Back", "Front", "Off (Double-Sided)" };
				int cullIdx = static_cast<int>(mat.cullMode);
				if (ImGui::Combo("Cull Mode##matCull", &cullIdx, cullItems, IM_ARRAYSIZE(cullItems)))
				{
					mat.cullMode = static_cast<MyEngine::CullMode>(cullIdx);
					renderEdited = true;
				}

				renderEdited |= ImGui::Checkbox("Depth Write##matDepthWrite", &mat.depthWrite);
				ImGui::SameLine();
				renderEdited |= ImGui::Checkbox("Depth Test##matDepthTest", &mat.depthTest);
				renderEdited |= ImGui::DragInt("Render Queue##matRQ", &mat.renderQueue, 1.0f, 0, 5000);
				ImGui::SameLine();
				ImGui::TextDisabled("(Opaque~2000, Transparent~3000)");
				if (renderEdited && ImGui::IsItemDeactivatedAfterEdit())
					pushMaterialUndo(beforeRenderSnapshot);
			}

			ImGui::Separator();
			if (ImGui::Button("Save##matbrowser"))
			{
				mat.shaderVertexPath = mat.shader ? mat.shader->GetVertexPath() : mat.shaderVertexPath;
				mat.shaderFragmentPath = mat.shader ? mat.shader->GetFragmentPath() : mat.shaderFragmentPath;
				mat.SaveToFile(ui.selectedMaterialPath);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save As...##matbrowser"))
			{
				std::string dest = MyEngine::FileDialog::SaveMaterialFile();
				if (!dest.empty())
				{
					mat.SetPath(dest);
					mat.shaderVertexPath = mat.shader ? mat.shader->GetVertexPath() : mat.shaderVertexPath;
					mat.shaderFragmentPath = mat.shader ? mat.shader->GetFragmentPath() : mat.shaderFragmentPath;
					mat.SaveToFile(dest);
					ui.selectedMaterialPath = dest;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Assign to Selected Entity##matbrowser"))
			{
				if (selectedEntity && selectedEntity->HasComponent<MeshRendererComponent>())
				{
					auto& mr = selectedEntity->GetComponent<MeshRendererComponent>();
					mr.material = ui.editingMaterial;
					mr.materialPath = ui.selectedMaterialPath;
					if (ui.editingMaterial->shader)
						mr.shader = ui.editingMaterial->shader;
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Select a material from the list or create a new one.");
		}

		ImGui::EndChild();
		ImGui::End();
	}
}
#endif
