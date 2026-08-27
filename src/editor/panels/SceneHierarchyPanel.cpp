#include "SceneHierarchyPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>

#include "components/TransformComponent.h"
#include "editor/EditorStyle.h"
#include "ecs/TransformHierarchy.h"

namespace MyEngine::Editor::Panels
{
	void DrawSceneHierarchyPanel(Context& context)
	{
		if (!context.ui || !context.scene || !context.selectedEntity)
			return;

		auto& ui = *context.ui;
		auto& scene = *context.scene;
		auto& selectedEntity = *context.selectedEntity;
		if (!ui.showSceneHierarchy)
			return;

		ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
		ImGui::Begin("Scene Hierarchy", &ui.showSceneHierarchy);

		static char hierarchyFilter[64] = "";
		auto matchesFilter = [&](const std::shared_ptr<Entity>& entity)
		{
			if (hierarchyFilter[0] == '\0')
				return true;
			std::string value = entity->GetName();
			if (!entity->GetTag().empty())
				value += " " + entity->GetTag();
			std::string filter = hierarchyFilter;
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value.find(filter) != std::string::npos;
		};

		size_t hierarchyEntityCount = 0;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity)
				++hierarchyEntityCount;
		}

		ImGui::TextDisabled("Entities: %zu", hierarchyEntityCount);
		if (selectedEntity)
			ImGui::TextDisabled("Selected: %s", selectedEntity->GetName().c_str());
		else
			ImGui::TextDisabled("Selected: none");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##hierarchyFilter", "Filter entities or tags", hierarchyFilter, sizeof(hierarchyFilter));

		if (InspectorActionButton("Create Empty Entity##hierarchy"))
		{
			const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
			auto ent = scene.CreateEntity("Entity");
			ent->AddComponent<TransformComponent>();
			selectedEntity = ent.get();
			const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
			if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
				context.pushSceneStateCommand(beforeState, afterState);
		}

		InspectorGroupLabel("Scene Graph");

		std::function<void(const std::shared_ptr<Entity>&)> drawEntityNode =
			[&](const std::shared_ptr<Entity>& entity)
		{
			bool hasChildren = false;
			for (auto& other : scene.GetEntities())
			{
				if (other && other != entity && other->HasComponent<TransformComponent>() &&
					other->GetComponent<TransformComponent>().parentID == entity->GetID())
				{
					hasChildren = true;
					break;
				}
			}

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
			if (!hasChildren)
				flags |= ImGuiTreeNodeFlags_Leaf;
			if (selectedEntity == entity.get())
				flags |= ImGuiTreeNodeFlags_Selected;
			if (!matchesFilter(entity))
				flags |= ImGuiTreeNodeFlags_Leaf;

			bool open = ImGui::TreeNodeEx(entity.get(), flags, "%s##%u", entity->GetName().c_str(), entity->GetID());

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(entity->GetName().c_str());
				ImGui::TextDisabled("ID: %u", entity->GetID());
				ImGui::TextDisabled("Layer: %u", entity->GetLayer());
				if (!entity->GetTag().empty())
					ImGui::TextDisabled("Tag: %s", entity->GetTag().c_str());
				ImGui::EndTooltip();
			}

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				selectedEntity = entity.get();

			if (ImGui::BeginDragDropSource())
			{
				uint32_t draggedID = entity->GetID();
				ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &draggedID, sizeof(uint32_t));
				ImGui::Text("%s", entity->GetName().c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
				{
					uint32_t draggedID = *static_cast<const uint32_t*>(payload->Data);
					auto dragged = TransformHierarchy::FindEntityByID(scene, draggedID);
					if (dragged && dragged.get() != entity.get())
					{
						const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
						TransformHierarchy::SetParent(scene, *dragged, entity->GetID());
						const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
						if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
							context.pushSceneStateCommand(beforeState, afterState);
					}
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Unparent", nullptr, false,
					entity->HasComponent<TransformComponent>() &&
					entity->GetComponent<TransformComponent>().parentID != 0))
				{
					TransformHierarchy::SetParent(scene, *entity, 0);
				}
				if (ImGui::MenuItem("Delete"))
				{
					const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
					uint32_t idToDelete = entity->GetID();
					if (selectedEntity == entity.get())
						selectedEntity = nullptr;
					for (auto& other : scene.GetEntities())
					{
						if (other && other->HasComponent<TransformComponent>() &&
							other->GetComponent<TransformComponent>().parentID == idToDelete)
							TransformHierarchy::SetParent(scene, *other, 0);
					}
					scene.DestroyEntity(idToDelete);
					const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
					if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
						context.pushSceneStateCommand(beforeState, afterState);
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				for (auto& child : scene.GetEntities())
				{
					if (child && child != entity && child->HasComponent<TransformComponent>() &&
						child->GetComponent<TransformComponent>().parentID == entity->GetID())
					{
						drawEntityNode(child);
					}
				}
				ImGui::TreePop();
			}
		};

		for (auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;
			uint32_t parentID = entity->HasComponent<TransformComponent>()
				? entity->GetComponent<TransformComponent>().parentID
				: 0;
			if (parentID == 0 && matchesFilter(entity))
				drawEntityNode(entity);
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Drag here to move an entity back to the root.");
		ImGui::Dummy(ImVec2(-1.0f, 30.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
			{
				uint32_t draggedID = *static_cast<const uint32_t*>(payload->Data);
				auto dragged = TransformHierarchy::FindEntityByID(scene, draggedID);
				if (dragged)
				{
					const std::string beforeState = context.captureSceneState ? context.captureSceneState() : std::string{};
					TransformHierarchy::SetParent(scene, *dragged, 0);
					const std::string afterState = context.captureSceneState ? context.captureSceneState() : std::string{};
					if (!beforeState.empty() && !afterState.empty() && beforeState != afterState && context.pushSceneStateCommand)
						context.pushSceneStateCommand(beforeState, afterState);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}
}
#endif
