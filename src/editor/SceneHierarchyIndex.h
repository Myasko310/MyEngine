#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"

namespace MyEngine::Editor
{
	// Per-frame snapshot: stable iteration during UI edits, with no persistent invalidation state.
	class SceneHierarchyIndex
	{
	public:
		using EntityList = std::vector<std::shared_ptr<Entity>>;

		explicit SceneHierarchyIndex(const Scene& scene)
		{
			m_Roots.reserve(scene.GetEntities().size());
			for (const auto& entity : scene.GetEntities())
			{
				if (!entity)
					continue;
				const uint32_t parentID = entity->HasComponent<TransformComponent>()
					? entity->GetComponent<TransformComponent>().parentID : 0;
				if (parentID == 0)
					m_Roots.push_back(entity);
				else if (parentID != entity->GetID())
					m_Children[parentID].push_back(entity);
			}
		}

		const EntityList& Roots() const { return m_Roots; }

		const EntityList& Children(uint32_t parentID) const
		{
			static const EntityList empty;
			const auto found = m_Children.find(parentID);
			return found == m_Children.end() ? empty : found->second;
		}

	private:
		EntityList m_Roots;
		std::unordered_map<uint32_t, EntityList> m_Children;
	};
}
