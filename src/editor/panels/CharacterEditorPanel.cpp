#include "CharacterEditorPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#ifdef USE_IMGUIZMO
#include "ImGuizmo.h"
#endif

#include "character/CharacterPreset.h"
#include "character/CharacterSocketSystem.h"
#include "components/AnimationComponent.h"
#include "components/CharacterCustomizationComponent.h"
#include "components/SkeletonComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"
#include "ecs/TransformHierarchy.h"
#include "rendering/Skeleton.h"

namespace
{
	bool ProjectWorldPointToScreen(
		const glm::vec3& worldPoint,
		const glm::mat4& view,
		const glm::mat4& projection,
		int windowWidth,
		int windowHeight,
		ImVec2& outScreen)
	{
		glm::vec3 projected = glm::project(
			worldPoint,
			view,
			projection,
			glm::vec4(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)));

		if (projected.z < 0.0f || projected.z > 1.0f)
			return false;

		outScreen = ImVec2(projected.x, static_cast<float>(windowHeight) - projected.y);
		return true;
	}

	// Resolves the entity being edited: the selected entity when it is
	// skinned, otherwise the player.
	Entity* ResolveTargetEntity(const MyEngine::Editor::Panels::CharacterEditorPanelContext& context)
	{
		if (context.selectedEntity && context.selectedEntity->HasComponent<SkeletonComponent>())
			return context.selectedEntity;
		if (context.playerEntity && *context.playerEntity && (*context.playerEntity)->HasComponent<SkeletonComponent>())
			return context.playerEntity->get();
		return nullptr;
	}

	// Model-space bone transforms (entity local space). Prefers the live
	// animated pose from AnimationComponent::boneMatrices; falls back to the
	// bind pose when no palette has been produced yet.
	std::vector<glm::mat4> BuildModelSpaceBoneTransforms(const Entity& entity, const MyEngine::Skeleton& skeleton)
	{
		const auto& bones = skeleton.GetBones();
		std::vector<glm::mat4> model(bones.size(), glm::mat4(1.0f));

		const std::vector<glm::mat4>* palette = nullptr;
		if (entity.HasComponent<AnimationComponent>())
		{
			const auto& anim = entity.GetComponent<AnimationComponent>();
			if (anim.boneMatrices.size() == bones.size())
				palette = &anim.boneMatrices;
		}

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const glm::mat4 inverseOffset = glm::inverse(bones[i].offsetMatrix);
			model[i] = palette ? (*palette)[i] * inverseOffset : inverseOffset;
		}
		return model;
	}

	void DrawBoneTreeNode(
		const MyEngine::Skeleton& skeleton,
		const std::vector<std::vector<int>>& children,
		const CharacterCustomizationComponent* customization,
		int boneIndex,
		int& selectedBoneIndex)
	{
		const auto& bone = skeleton.GetBones()[boneIndex];
		const bool hasChildren = !children[boneIndex].empty();
		const bool hasOverride = customization && customization->FindOverride(bone.name) && !customization->FindOverride(bone.name)->IsIdentity();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (selectedBoneIndex == boneIndex)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (hasOverride)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.35f, 1.0f));
		const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(boneIndex)), flags, "%s", bone.name.c_str());
		if (hasOverride)
			ImGui::PopStyleColor();

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			selectedBoneIndex = boneIndex;

		if (open && hasChildren)
		{
			for (int child : children[boneIndex])
				DrawBoneTreeNode(skeleton, children, customization, child, selectedBoneIndex);
			ImGui::TreePop();
		}
	}

	void DrawSkeletonOverlay(
		const MyEngine::Editor::Panels::CharacterEditorPanelContext& context,
		const MyEngine::Skeleton& skeleton,
		const std::vector<glm::mat4>& modelBones,
		const glm::mat4& entityWorld,
		int selectedBoneIndex,
		bool drawNames)
	{
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const auto& bones = skeleton.GetBones();
		const ImU32 boneColor = IM_COL32(90, 200, 255, 220);
		const ImU32 jointColor = IM_COL32(255, 255, 255, 230);
		const ImU32 selectedColor = IM_COL32(255, 170, 40, 255);

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const glm::vec3 jointWorld = glm::vec3(entityWorld * modelBones[i] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			ImVec2 jointScreen;
			if (!ProjectWorldPointToScreen(jointWorld, context.viewMatrix, context.projectionMatrix, context.viewportWidth, context.viewportHeight, jointScreen))
				continue;

			if (bones[i].parentIndex >= 0)
			{
				const glm::vec3 parentWorld = glm::vec3(entityWorld * modelBones[bones[i].parentIndex] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
				ImVec2 parentScreen;
				if (ProjectWorldPointToScreen(parentWorld, context.viewMatrix, context.projectionMatrix, context.viewportWidth, context.viewportHeight, parentScreen))
					drawList->AddLine(parentScreen, jointScreen, static_cast<int>(i) == selectedBoneIndex ? selectedColor : boneColor, 2.0f);
			}

			const bool selected = static_cast<int>(i) == selectedBoneIndex;
			drawList->AddCircleFilled(jointScreen, selected ? 5.0f : 3.0f, selected ? selectedColor : jointColor);
			if (drawNames || selected)
				drawList->AddText(ImVec2(jointScreen.x + 6.0f, jointScreen.y - 6.0f), selected ? selectedColor : IM_COL32(220, 220, 220, 200), bones[i].name.c_str());
		}
	}

	void DrawOverrideEditor(CharacterCustomizationComponent& customization, const std::string& boneName)
	{
		BoneOverride* existing = const_cast<BoneOverride*>(customization.FindOverride(boneName));
		BoneOverride working = existing ? *existing : BoneOverride{};
		bool changed = false;

		changed |= ImGui::DragFloat3("Position Offset", glm::value_ptr(working.positionOffset), 0.005f, -5.0f, 5.0f, "%.3f");
		changed |= ImGui::DragFloat3("Rotation Offset (deg)", glm::value_ptr(working.rotationOffsetDegrees), 0.5f, -180.0f, 180.0f, "%.1f");
		changed |= ImGui::DragFloat3("Scale Multiplier", glm::value_ptr(working.scaleMultiplier), 0.005f, 0.05f, 5.0f, "%.3f");

		float uniform = (working.scaleMultiplier.x + working.scaleMultiplier.y + working.scaleMultiplier.z) / 3.0f;
		if (ImGui::DragFloat("Uniform Scale", &uniform, 0.005f, 0.05f, 5.0f, "%.3f"))
		{
			working.scaleMultiplier = glm::vec3(uniform);
			changed = true;
		}
		changed |= ImGui::Checkbox("Counter-scale children", &working.counterScaleChildren);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Child bones inverse-apply this bone's scale so only this segment changes size.");

		if (changed)
			customization.GetOrCreateOverride(boneName) = working;

		if (ImGui::Button("Reset Bone"))
			customization.ResetOverride(boneName);
	}

	void DrawBodySliders(
		MyEngine::Editor::Panels::CharacterEditorPanelState& state,
		CharacterCustomizationComponent& customization,
		const MyEngine::Skeleton& skeleton)
	{
		if (customization.bodySliders.empty())
		{
			if (ImGui::Button("Add Default Body Sliders"))
				customization.EnsureDefaultBodySliders();
			ImGui::SameLine();
			ImGui::Checkbox("Mapping Editor", &state.showSliderMappingEditor);
			return;
		}

		if (ImGui::Button("Reset Sliders"))
		{
			for (auto& slider : customization.bodySliders)
				slider.value = 0.0f;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Mapping Editor", &state.showSliderMappingEditor);

		if (state.editingSliderIndex >= static_cast<int>(customization.bodySliders.size()))
			state.editingSliderIndex = -1;

		for (size_t i = 0; i < customization.bodySliders.size(); ++i)
		{
			auto& slider = customization.bodySliders[i];
			int resolved = 0;
			for (const auto& mapping : slider.mappings)
				resolved += skeleton.GetBoneIndex(mapping.boneName) >= 0 ? 1 : 0;
			const bool unmapped = slider.mappings.empty() || resolved == 0;

			ImGui::PushID(static_cast<int>(i));
			if (state.showSliderMappingEditor)
			{
				if (ImGui::RadioButton("##edit", state.editingSliderIndex == static_cast<int>(i)))
				{
					state.editingSliderIndex = static_cast<int>(i);
					state.selectedMappingIndex = -1;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Edit this slider's bone mappings");
				ImGui::SameLine();
			}

			if (unmapped)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			const std::string label = slider.customMappings ? slider.name + "*" : slider.name;
			ImGui::SliderFloat(label.c_str(), &slider.value, slider.minValue, slider.maxValue, "%.2f");
			if (unmapped)
				ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("%d/%zu mapped bones%s", resolved, slider.mappings.size(), slider.customMappings ? "  (custom)" : "");
				for (const auto& mapping : slider.mappings)
					ImGui::BulletText("%s%s", mapping.boneName.c_str(), skeleton.GetBoneIndex(mapping.boneName) >= 0 ? "" : " (missing)");
				ImGui::EndTooltip();
			}
			ImGui::PopID();
		}
	}

	// Per-mapping editor: lets the user choose which bones a slider drives and
	// how much position/rotation/scale one slider unit contributes.
	void DrawSliderMappingEditor(
		MyEngine::Editor::Panels::CharacterEditorPanelState& state,
		CharacterCustomizationComponent& customization,
		const MyEngine::Skeleton& skeleton)
	{
		const auto& bones = skeleton.GetBones();

		ImGui::InputText("##newSliderName", state.newSliderNameBuffer, sizeof(state.newSliderNameBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Add Slider"))
		{
			std::string baseName = state.newSliderNameBuffer;
			if (baseName.empty())
				baseName = "Custom Slider";
			std::string name = baseName;
			for (int suffix = 2; customization.FindSlider(name) != nullptr; ++suffix)
				name = baseName + " " + std::to_string(suffix);
			if (customization.AddCustomSlider(name))
			{
				state.editingSliderIndex = static_cast<int>(customization.bodySliders.size()) - 1;
				state.selectedMappingIndex = -1;
			}
		}

		if (state.editingSliderIndex < 0 || state.editingSliderIndex >= static_cast<int>(customization.bodySliders.size()))
		{
			ImGui::TextDisabled("Pick a slider (radio button) above to edit its mappings.");
			return;
		}

		BodySlider& slider = customization.bodySliders[state.editingSliderIndex];
		ImGui::PushID("sliderMappingEdit");

		char nameBuffer[64];
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", slider.name.c_str());
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			const std::string requested = nameBuffer;
			if (!requested.empty() && (customization.FindSlider(requested) == nullptr || requested == slider.name))
			{
				slider.name = requested;
				slider.customMappings = true;
			}
		}

		float range[2] = { slider.minValue, slider.maxValue };
		if (ImGui::DragFloat2("Range (min/max)", range, 0.01f, -10.0f, 10.0f, "%.2f"))
		{
			slider.minValue = std::min(range[0], range[1]);
			slider.maxValue = std::max(range[0], range[1]);
			slider.value = std::clamp(slider.value, slider.minValue, slider.maxValue);
			slider.customMappings = true;
		}

		if (ImGui::Button("Reset To Default Mapping"))
		{
			if (!customization.ResetSliderToDefault(slider.name))
				state.statusMessage = "'" + slider.name + "' has no rig default mapping";
			state.selectedMappingIndex = -1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove Slider"))
		{
			customization.bodySliders.erase(customization.bodySliders.begin() + state.editingSliderIndex);
			state.editingSliderIndex = -1;
			state.selectedMappingIndex = -1;
			ImGui::PopID();
			return;
		}

		ImGui::Separator();
		ImGui::Text("Bone Mappings (%zu)", slider.mappings.size());

		const bool hasSelectedBone = state.selectedBoneIndex >= 0 && state.selectedBoneIndex < static_cast<int>(bones.size());
		if (ImGui::Button("Add Mapping"))
		{
			BodySliderBoneMapping mapping;
			mapping.boneName = hasSelectedBone ? bones[state.selectedBoneIndex].name : (bones.empty() ? std::string() : bones.front().name);
			slider.mappings.push_back(std::move(mapping));
			slider.customMappings = true;
			state.selectedMappingIndex = static_cast<int>(slider.mappings.size()) - 1;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Adds a mapping for the bone selected in the hierarchy (or the root bone).");
		if (hasSelectedBone && state.selectedMappingIndex >= 0 && state.selectedMappingIndex < static_cast<int>(slider.mappings.size()))
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Use Selected Bone"))
			{
				slider.mappings[state.selectedMappingIndex].boneName = bones[state.selectedBoneIndex].name;
				slider.customMappings = true;
			}
		}

		if (state.selectedMappingIndex >= static_cast<int>(slider.mappings.size()))
			state.selectedMappingIndex = -1;

		ImGui::BeginChild("##mappingList", ImVec2(0, 90), true);
		for (size_t i = 0; i < slider.mappings.size(); ++i)
		{
			const auto& mapping = slider.mappings[i];
			const bool missing = skeleton.GetBoneIndex(mapping.boneName) < 0;
			ImGui::PushID(static_cast<int>(i));
			if (missing)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			const std::string label = mapping.boneName + (missing ? " (missing)" : "");
			if (ImGui::Selectable(label.c_str(), state.selectedMappingIndex == static_cast<int>(i)))
			{
				state.selectedMappingIndex = static_cast<int>(i);
				const int boneIndex = skeleton.GetBoneIndex(mapping.boneName);
				if (boneIndex >= 0)
					state.selectedBoneIndex = boneIndex;
			}
			if (missing)
				ImGui::PopStyleColor();
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (state.selectedMappingIndex >= 0)
		{
			BodySliderBoneMapping& mapping = slider.mappings[state.selectedMappingIndex];
			ImGui::PushID("mappingEdit");
			bool changed = false;

			const int currentBone = skeleton.GetBoneIndex(mapping.boneName);
			if (ImGui::BeginCombo("Bone", currentBone >= 0 ? mapping.boneName.c_str() : (mapping.boneName + " (missing)").c_str()))
			{
				for (size_t i = 0; i < bones.size(); ++i)
				{
					if (ImGui::Selectable(bones[i].name.c_str(), static_cast<int>(i) == currentBone))
					{
						mapping.boneName = bones[i].name;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}

			changed |= ImGui::DragFloat3("Position / unit", glm::value_ptr(mapping.positionPerUnit), 0.005f, -5.0f, 5.0f, "%.3f");
			changed |= ImGui::DragFloat3("Rotation (deg) / unit", glm::value_ptr(mapping.rotationDegreesPerUnit), 0.5f, -180.0f, 180.0f, "%.1f");
			changed |= ImGui::DragFloat3("Scale / unit", glm::value_ptr(mapping.scalePerUnit), 0.005f, -0.95f, 5.0f, "%.3f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Added to 1.0 per slider unit (0.2 => 1.2x at value 1). Scale propagates to child bones.");
			changed |= ImGui::Checkbox("Counter-scale children", &mapping.counterScaleChildren);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Cancels this mapping's scale on child bones (e.g. thicken the arm without thickening the hand).");

			if (ImGui::Button("Remove Mapping"))
			{
				slider.mappings.erase(slider.mappings.begin() + state.selectedMappingIndex);
				state.selectedMappingIndex = -1;
				changed = true;
			}
			if (changed)
				slider.customMappings = true;
			ImGui::PopID();
		}

		ImGui::PopID();
	}

	void DrawSocketEditor(
		MyEngine::Editor::Panels::CharacterEditorPanelState& state,
		CharacterCustomizationComponent& customization,
		const MyEngine::Skeleton& skeleton)
	{
		const auto& bones = skeleton.GetBones();

		ImGui::InputText("##newSocketName", state.newSocketNameBuffer, sizeof(state.newSocketNameBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Add Socket"))
		{
			std::string baseName = state.newSocketNameBuffer;
			if (baseName.empty())
				baseName = "Socket";
			std::string name = baseName;
			for (int suffix = 2; customization.FindSocket(name) != nullptr; ++suffix)
				name = baseName + "_" + std::to_string(suffix);

			BoneSocket socket;
			socket.name = name;
			socket.boneName = state.selectedBoneIndex >= 0 && state.selectedBoneIndex < static_cast<int>(bones.size())
				? bones[state.selectedBoneIndex].name
				: (bones.empty() ? std::string() : bones.front().name);
			socket.meshPath = "primitive_cube";
			socket.scale = glm::vec3(0.1f);
			customization.sockets.push_back(socket);
			state.selectedSocketIndex = static_cast<int>(customization.sockets.size()) - 1;
		}

		if (customization.sockets.empty())
		{
			ImGui::TextDisabled("No sockets. Select a bone and press Add Socket.");
			return;
		}

		if (state.selectedSocketIndex >= static_cast<int>(customization.sockets.size()))
			state.selectedSocketIndex = -1;

		ImGui::BeginChild("##socketList", ImVec2(0, 90), true);
		for (size_t i = 0; i < customization.sockets.size(); ++i)
		{
			const auto& socket = customization.sockets[i];
			ImGui::PushID(static_cast<int>(i));
			const std::string label = socket.name + "  [" + socket.boneName + "]";
			if (ImGui::Selectable(label.c_str(), state.selectedSocketIndex == static_cast<int>(i)))
				state.selectedSocketIndex = static_cast<int>(i);
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (state.selectedSocketIndex < 0)
			return;

		BoneSocket& socket = customization.sockets[state.selectedSocketIndex];
		ImGui::PushID("socketEdit");

		char nameBuffer[64];
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", socket.name.c_str());
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			const std::string requested = nameBuffer;
			if (!requested.empty() && (customization.FindSocket(requested) == nullptr || requested == socket.name))
				socket.name = requested;
		}

		const int currentBone = skeleton.GetBoneIndex(socket.boneName);
		if (ImGui::BeginCombo("Bone", currentBone >= 0 ? socket.boneName.c_str() : (socket.boneName + " (missing)").c_str()))
		{
			for (size_t i = 0; i < bones.size(); ++i)
			{
				if (ImGui::Selectable(bones[i].name.c_str(), static_cast<int>(i) == currentBone))
					socket.boneName = bones[i].name;
			}
			ImGui::EndCombo();
		}
		if (state.selectedBoneIndex >= 0 && state.selectedBoneIndex < static_cast<int>(bones.size()))
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Use Selected Bone"))
				socket.boneName = bones[state.selectedBoneIndex].name;
		}

		char meshBuffer[512];
		std::snprintf(meshBuffer, sizeof(meshBuffer), "%s", socket.meshPath.c_str());
		if (ImGui::InputText("Mesh", meshBuffer, sizeof(meshBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
			socket.meshPath = meshBuffer;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Model file (fbx/obj/gltf) or primitive_cube / primitive_sphere / primitive_plane. Press Enter to apply.");

		ImGui::DragFloat3("Position", glm::value_ptr(socket.positionOffset), 0.005f, -5.0f, 5.0f, "%.3f");
		ImGui::DragFloat3("Rotation (deg)", glm::value_ptr(socket.rotationOffsetDegrees), 0.5f, -180.0f, 180.0f, "%.1f");
		ImGui::DragFloat3("Scale", glm::value_ptr(socket.scale), 0.005f, 0.001f, 100.0f, "%.3f");
		ImGui::Checkbox("Visible", &socket.visible);
		ImGui::SameLine();
		if (ImGui::Button("Remove Socket"))
		{
			customization.sockets.erase(customization.sockets.begin() + state.selectedSocketIndex);
			state.selectedSocketIndex = -1;
		}
		ImGui::PopID();
	}

	void DrawSocketOverlay(
		const MyEngine::Editor::Panels::CharacterEditorPanelContext& context,
		const Entity& target,
		const CharacterCustomizationComponent& customization,
		int selectedSocketIndex)
	{
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		for (size_t i = 0; i < customization.sockets.size(); ++i)
		{
			const auto& socket = customization.sockets[i];
			glm::mat4 world;
			if (!MyEngine::CharacterSocketSystem::ComputeSocketWorldMatrix(*context.scene, target, socket.boneName, socket.BuildLocalMatrix(), world))
				continue;

			ImVec2 screen;
			if (!ProjectWorldPointToScreen(glm::vec3(world[3]), context.viewMatrix, context.projectionMatrix, context.viewportWidth, context.viewportHeight, screen))
				continue;

			const bool selected = static_cast<int>(i) == selectedSocketIndex;
			const ImU32 color = selected ? IM_COL32(120, 255, 120, 255) : IM_COL32(80, 200, 80, 200);
			drawList->AddRect(ImVec2(screen.x - 5.0f, screen.y - 5.0f), ImVec2(screen.x + 5.0f, screen.y + 5.0f), color, 0.0f, 0, selected ? 2.0f : 1.0f);
			drawList->AddText(ImVec2(screen.x + 7.0f, screen.y + 2.0f), color, socket.name.c_str());
		}
	}

	void RefreshPresetList(MyEngine::Editor::Panels::CharacterEditorPanelState& state)
	{
		const std::string previous = state.selectedPresetIndex >= 0 && state.selectedPresetIndex < static_cast<int>(state.presetList.size())
			? state.presetList[state.selectedPresetIndex]
			: std::string();
		state.presetList = MyEngine::CharacterPreset::ListPresets(state.presetDirectoryBuffer);
		state.selectedPresetIndex = -1;
		for (size_t i = 0; i < state.presetList.size(); ++i)
		{
			if (state.presetList[i] == previous)
				state.selectedPresetIndex = static_cast<int>(i);
		}
		state.presetListDirty = false;
	}

	void SelectPreset(MyEngine::Editor::Panels::CharacterEditorPanelState& state, int index)
	{
		state.selectedPresetIndex = index;
		if (index >= 0 && index < static_cast<int>(state.presetList.size()))
		{
			std::snprintf(state.presetPathBuffer, sizeof(state.presetPathBuffer), "%s", state.presetList[index].c_str());
			std::snprintf(state.presetNameBuffer, sizeof(state.presetNameBuffer), "%s", MyEngine::CharacterPreset::PresetDisplayName(state.presetList[index]).c_str());
		}
	}

	void DrawPresetBrowser(
		MyEngine::Editor::Panels::CharacterEditorPanelState& state,
		CharacterCustomizationComponent& customization,
		const MyEngine::Skeleton& skeleton)
	{
		namespace Preset = MyEngine::CharacterPreset;

		if (state.presetListDirty)
			RefreshPresetList(state);

		ImGui::SetNextItemWidth(-90.0f);
		if (ImGui::InputText("##presetDir", state.presetDirectoryBuffer, sizeof(state.presetDirectoryBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
			state.presetListDirty = true;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Preset folder. Press Enter to rescan.");
		ImGui::SameLine();
		if (ImGui::Button("Refresh"))
			state.presetListDirty = true;

		if (state.selectedPresetIndex >= static_cast<int>(state.presetList.size()))
			state.selectedPresetIndex = -1;

		ImGui::BeginChild("##presetList", ImVec2(0, 110), true);
		if (state.presetList.empty())
			ImGui::TextDisabled("No *.character.json presets in this folder.");
		for (size_t i = 0; i < state.presetList.size(); ++i)
		{
			const std::string& path = state.presetList[i];
			const bool isActive = !customization.presetPath.empty() && customization.presetPath == path;
			ImGui::PushID(static_cast<int>(i));
			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 1.0f, 0.55f, 1.0f));
			const std::string label = Preset::PresetDisplayName(path) + (isActive ? "  (active)" : "");
			if (ImGui::Selectable(label.c_str(), state.selectedPresetIndex == static_cast<int>(i), ImGuiSelectableFlags_AllowDoubleClick))
			{
				SelectPreset(state, static_cast<int>(i));
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					state.statusMessage = Preset::Load(path, customization) ? "Loaded " + path : "Failed to load " + path;
			}
			if (isActive)
				ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s\nDouble-click to load.", path.c_str());
			ImGui::PopID();
		}
		ImGui::EndChild();

		const bool hasSelection = state.selectedPresetIndex >= 0;
		const std::string selectedPath = hasSelection ? state.presetList[state.selectedPresetIndex] : std::string();

		ImGui::SetNextItemWidth(-90.0f);
		ImGui::InputText("##presetName", state.presetNameBuffer, sizeof(state.presetNameBuffer));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Name used by Save As / Duplicate / Rename.");
		ImGui::SameLine();
		if (ImGui::Button("Save As"))
		{
			const std::string path = Preset::MakePresetPath(state.presetDirectoryBuffer, state.presetNameBuffer);
			if (path.empty())
			{
				state.statusMessage = "Enter a preset name first";
			}
			else if (Preset::Save(customization, path, &skeleton))
			{
				customization.presetPath = path;
				std::snprintf(state.presetPathBuffer, sizeof(state.presetPathBuffer), "%s", path.c_str());
				state.statusMessage = "Saved " + path;
				state.presetListDirty = true;
			}
			else
			{
				state.statusMessage = "Failed to save " + path;
			}
		}

		ImGui::BeginDisabled(!hasSelection);
		if (ImGui::Button("Load"))
			state.statusMessage = Preset::Load(selectedPath, customization) ? "Loaded " + selectedPath : "Failed to load " + selectedPath;
		ImGui::SameLine();
		if (ImGui::Button("Overwrite"))
		{
			if (Preset::Save(customization, selectedPath, &skeleton))
			{
				customization.presetPath = selectedPath;
				state.statusMessage = "Saved " + selectedPath;
			}
			else
			{
				state.statusMessage = "Failed to save " + selectedPath;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Duplicate"))
		{
			const std::string requested = state.presetNameBuffer;
			const bool sameName = requested == Preset::PresetDisplayName(selectedPath);
			const std::string created = Preset::Duplicate(selectedPath, sameName ? std::string() : requested);
			if (created.empty())
			{
				state.statusMessage = "Duplicate failed (name in use?)";
			}
			else
			{
				state.statusMessage = "Created " + created;
				RefreshPresetList(state);
				for (size_t i = 0; i < state.presetList.size(); ++i)
					if (state.presetList[i] == created)
						SelectPreset(state, static_cast<int>(i));
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Rename"))
		{
			const std::string renamed = Preset::Rename(selectedPath, state.presetNameBuffer);
			if (renamed.empty())
			{
				state.statusMessage = "Rename failed (empty or name in use)";
			}
			else
			{
				if (customization.presetPath == selectedPath)
					customization.presetPath = renamed;
				state.statusMessage = "Renamed to " + renamed;
				RefreshPresetList(state);
				for (size_t i = 0; i < state.presetList.size(); ++i)
					if (state.presetList[i] == renamed)
						SelectPreset(state, static_cast<int>(i));
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete"))
		{
			state.pendingDeletePresetIndex = state.selectedPresetIndex;
			ImGui::OpenPopup("Delete Preset?");
		}
		ImGui::EndDisabled();

		if (ImGui::BeginPopupModal("Delete Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const bool valid = state.pendingDeletePresetIndex >= 0 && state.pendingDeletePresetIndex < static_cast<int>(state.presetList.size());
			const std::string target = valid ? state.presetList[state.pendingDeletePresetIndex] : std::string();
			ImGui::Text("Permanently delete this preset file?");
			ImGui::TextDisabled("%s", target.c_str());
			ImGui::Separator();
			if (ImGui::Button("Delete", ImVec2(120, 0)) && valid)
			{
				if (Preset::Delete(target))
				{
					if (customization.presetPath == target)
						customization.presetPath.clear();
					state.statusMessage = "Deleted " + target;
				}
				else
				{
					state.statusMessage = "Failed to delete " + target;
				}
				state.pendingDeletePresetIndex = -1;
				state.selectedPresetIndex = -1;
				state.presetListDirty = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				state.pendingDeletePresetIndex = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

#ifdef USE_IMGUIZMO
	void DrawBoneGizmo(
		MyEngine::Editor::Panels::CharacterEditorPanelContext& context,
		MyEngine::Editor::Panels::CharacterEditorPanelState& state,
		CharacterCustomizationComponent& customization,
		const MyEngine::Skeleton& skeleton,
		const std::vector<glm::mat4>& modelBones,
		const glm::mat4& entityWorld)
	{
		const auto& bones = skeleton.GetBones();
		const int boneIndex = state.selectedBoneIndex;
		if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()))
			return;

		const MyEngine::Bone& bone = bones[boneIndex];
		BoneOverride& boneOverride = customization.GetOrCreateOverride(bone.name);
		const glm::mat4 sliderDelta = customization.ComputeSliderOverride(bone.name).BuildDeltaMatrix();

		// Decompose the live pose into (parentWorld, baseLocal) where baseLocal
		// is the animated local with the slider delta included, so the gizmo
		// edits only the manual override: modelBone ~= parent * baseLocal * manualDelta.
		// Slider and manual deltas are combined component-wise in the effective
		// override, so this factoring is exact for scale/translation and a close
		// approximation when both carry rotation.
		const glm::mat4 parentModel = bone.parentIndex >= 0 ? modelBones[bone.parentIndex] : glm::mat4(1.0f);
		const glm::mat4 parentWorld = entityWorld * parentModel;
		const glm::mat4 localWithOverride = glm::inverse(parentModel) * modelBones[boneIndex];
		const glm::mat4 animatedLocal = localWithOverride * glm::inverse(customization.ComputeEffectiveOverride(bone.name).BuildDeltaMatrix()) * sliderDelta;

		// While dragging, freeze the animated base so the running animation
		// doesn't fight the gizmo and the whole drag reads as a single edit.
		if (!state.gizmoWasUsing)
		{
			state.gizmoDragStartParentWorld = parentWorld;
			state.gizmoDragStartAnimatedLocal = animatedLocal;
		}
		const glm::mat4 baseWorld = state.gizmoDragStartParentWorld * state.gizmoDragStartAnimatedLocal;
		glm::mat4 gizmoMatrix = baseWorld * boneOverride.BuildDeltaMatrix();

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::BeginFrame();
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(context.viewportWidth), static_cast<float>(context.viewportHeight));

		ImGuizmo::Manipulate(
			glm::value_ptr(context.viewMatrix),
			glm::value_ptr(context.projectionMatrix),
			static_cast<ImGuizmo::OPERATION>(context.gizmoOperation),
			static_cast<ImGuizmo::MODE>(context.gizmoMode),
			glm::value_ptr(gizmoMatrix));

		context.boneGizmoActive = true;

		if (ImGuizmo::IsUsing())
		{
			state.gizmoWasUsing = true;
			const glm::mat4 newDelta = glm::inverse(baseWorld) * gizmoMatrix;

			float translation[3];
			float rotation[3];
			float scale[3];
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(newDelta), translation, rotation, scale);

			boneOverride.positionOffset = glm::vec3(translation[0], translation[1], translation[2]);
			boneOverride.rotationOffsetDegrees = glm::vec3(rotation[0], rotation[1], rotation[2]);
			boneOverride.scaleMultiplier = glm::max(glm::vec3(scale[0], scale[1], scale[2]), glm::vec3(0.01f));
		}
		else
		{
			state.gizmoWasUsing = false;
			if (boneOverride.IsIdentity())
				customization.ResetOverride(bone.name);
		}
	}
#endif
}

namespace MyEngine::Editor::Panels
{
	void DrawCharacterEditorPanel(CharacterEditorPanelContext& context)
	{
		context.boneGizmoActive = false;
		if (!context.isOpen || !*context.isOpen || !context.scene || !context.state)
			return;

		CharacterEditorPanelState& state = *context.state;

		ImGui::SetNextWindowPos(ImVec2(760, 80), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420, 620), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Character Editor", context.isOpen))
		{
			ImGui::End();
			return;
		}

		Entity* target = ResolveTargetEntity(context);
		if (!target)
		{
			ImGui::TextWrapped("No skinned character found. Select an entity with a SkeletonComponent or load a scene with a Player.");
			ImGui::End();
			return;
		}

		auto& skeletonComponent = target->GetComponent<SkeletonComponent>();
		if (!skeletonComponent.skeleton || skeletonComponent.skeleton->GetBoneCount() == 0)
		{
			ImGui::TextWrapped("Entity '%s' has an empty skeleton.", target->GetName().c_str());
			ImGui::End();
			return;
		}

		const Skeleton& skeleton = *skeletonComponent.skeleton;
		const auto& bones = skeleton.GetBones();
		if (!target->HasComponent<CharacterCustomizationComponent>())
			target->AddComponent<CharacterCustomizationComponent>();
		auto& customization = target->GetComponent<CharacterCustomizationComponent>();

		if (state.selectedBoneIndex >= static_cast<int>(bones.size()))
			state.selectedBoneIndex = -1;

		ImGui::Text("Target: %s", target->GetName().c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("(%zu bones)", bones.size());

		ImGui::Checkbox("Enable Overrides", &customization.enabled);
		ImGui::SameLine();
		ImGui::Checkbox("Skeleton Overlay", &state.drawSkeletonOverlay);
		ImGui::SameLine();
		ImGui::Checkbox("Bone Names", &state.drawBoneNames);
		ImGui::SameLine();
		ImGui::Checkbox("Sockets", &state.drawSocketOverlay);
#ifdef USE_IMGUIZMO
		ImGui::Checkbox("Bone Gizmo", &state.boneGizmoEnabled);
		ImGui::SameLine();
		ImGui::TextDisabled("(uses the toolbar's gizmo operation/mode)");
#endif

		ImGui::Separator();
		ImGui::Text("Preset");
		ImGui::SameLine();
		ImGui::Checkbox("Browser", &state.showPresetBrowser);
		if (!customization.presetPath.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("active: %s", CharacterPreset::PresetDisplayName(customization.presetPath).c_str());
		}
		ImGui::InputText("##presetPath", state.presetPathBuffer, sizeof(state.presetPathBuffer));
		if (ImGui::Button("Save"))
		{
			const std::string path = state.presetPathBuffer;
			if (CharacterPreset::Save(customization, path, &skeleton))
			{
				customization.presetPath = path;
				state.statusMessage = "Saved " + path;
				state.presetListDirty = true;
			}
			else
			{
				state.statusMessage = "Failed to save " + path;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			const std::string path = state.presetPathBuffer;
			state.statusMessage = CharacterPreset::Load(path, customization) ? "Loaded " + path : "Failed to load " + path;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset All"))
		{
			customization.ResetAll();
			state.statusMessage = "Cleared all bone overrides and sliders";
		}
		if (!state.statusMessage.empty())
			ImGui::TextWrapped("%s", state.statusMessage.c_str());

		if (state.showPresetBrowser)
		{
			ImGui::Indent();
			DrawPresetBrowser(state, customization, skeleton);
			ImGui::Unindent();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Body Sliders", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawBodySliders(state, customization, skeleton);
			if (state.showSliderMappingEditor)
			{
				ImGui::Indent();
				ImGui::SeparatorText("Slider Mapping Editor");
				DrawSliderMappingEditor(state, customization, skeleton);
				ImGui::Unindent();
			}
		}

		if (ImGui::CollapsingHeader("Sockets"))
			DrawSocketEditor(state, customization, skeleton);

		// Build child lists once per frame for the hierarchy view.
		std::vector<std::vector<int>> children(bones.size());
		std::vector<int> roots;
		for (size_t i = 0; i < bones.size(); ++i)
		{
			const int parent = bones[i].parentIndex;
			if (parent >= 0 && parent < static_cast<int>(bones.size()))
				children[parent].push_back(static_cast<int>(i));
			else
				roots.push_back(static_cast<int>(i));
		}

		const float hierarchyHeight = std::max(140.0f, ImGui::GetContentRegionAvail().y * 0.45f);
		ImGui::Text("Bone Hierarchy");
		ImGui::BeginChild("##boneHierarchy", ImVec2(0, hierarchyHeight), true);
		for (int root : roots)
			DrawBoneTreeNode(skeleton, children, &customization, root, state.selectedBoneIndex);
		ImGui::EndChild();

		ImGui::Separator();
		if (state.selectedBoneIndex >= 0)
		{
			const Bone& bone = bones[state.selectedBoneIndex];
			ImGui::Text("Bone: %s", bone.name.c_str());
			if (bone.parentIndex >= 0)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(parent: %s)", bones[bone.parentIndex].name.c_str());
			}
			DrawOverrideEditor(customization, bone.name);
		}
		else
		{
			ImGui::TextDisabled("Select a bone in the hierarchy to edit it.");
		}

		ImGui::End();

		// Viewport overlays run after the window so they draw regardless of
		// how the panel itself is laid out.
		const glm::mat4 entityWorld = target->HasComponent<TransformComponent>()
			? TransformHierarchy::GetWorldMatrix(*context.scene, *target)
			: glm::mat4(1.0f);
		const std::vector<glm::mat4> modelBones = BuildModelSpaceBoneTransforms(*target, skeleton);

		if (state.drawSkeletonOverlay)
			DrawSkeletonOverlay(context, skeleton, modelBones, entityWorld, state.selectedBoneIndex, state.drawBoneNames);
		if (state.drawSocketOverlay && !customization.sockets.empty())
			DrawSocketOverlay(context, *target, customization, state.selectedSocketIndex);

#ifdef USE_IMGUIZMO
		if (state.boneGizmoEnabled && state.selectedBoneIndex >= 0)
			DrawBoneGizmo(context, state, customization, skeleton, modelBones, entityWorld);
		else
			state.gizmoWasUsing = false;
#endif
	}
}
#else
namespace MyEngine::Editor::Panels
{
	void DrawCharacterEditorPanel(CharacterEditorPanelContext& context)
	{
		context.boneGizmoActive = false;
	}
}
#endif
