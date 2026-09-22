#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "systems/PhysicsSystem.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <utility>
#include <vector>

bool ArenaBroadphaseSmokeTest()
{
	using Grid = MyEngine::PhysicsSystem::BroadphaseGrid;
	struct Entry
	{
		std::shared_ptr<Entity> entity;
		glm::vec3 min;
		glm::vec3 max;
	};
	Scene scene;
	std::vector<Entry> entries;
	auto add = [&](const glm::vec3& min, const glm::vec3& max)
	{
		entries.push_back({ scene.CreateEntity("BroadphaseProbe"), min, max });
	};
	add(glm::vec3(-32, -0.5f, -50), glm::vec3(32, 0, 14));
	add(glm::vec3(-32, 0, -50), glm::vec3(-31, 4, 14));
	add(glm::vec3(-0.25f, 0, -20.25f), glm::vec3(0.25f, 0.5f, -19.75f));
	add(glm::vec3(0), glm::vec3(6)); // Exactly 64 cells at the default cell size.
	add(glm::vec3(0), glm::vec3(8)); // Cross the oversized threshold.
	add(glm::vec3(6), glm::vec3(6.5f)); // Inclusive boundary contact.
	add(glm::vec3(-10000), glm::vec3(10000));
	add(glm::vec3(20000), glm::vec3(20010));

	std::mt19937 random(310);
	std::uniform_real_distribution<float> position(-40.0f, 40.0f);
	std::uniform_real_distribution<float> extent(0.05f, 2.0f);
	for (int i = 0; i < 80; ++i)
	{
		const glm::vec3 center(position(random), position(random), position(random));
		const glm::vec3 half = i % 8 == 0 ? glm::vec3(12) : glm::vec3(extent(random), extent(random), extent(random));
		add(center - half, center + half);
	}

	Grid grid;
	for (const float cellSize : { 1.0f, 2.0f, 4.0f })
	{
		grid.cellSize = cellSize;
		for (int round = 0; round < 3; ++round)
		{
			grid.Clear();
			int staleCallbacks = 0;
			grid.ForEachCandidatePair([&](const auto&, const auto&) { ++staleCallbacks; });
			if (staleCallbacks != 0 || !grid.cells.empty())
				return false;

			// Exercise rebuilds after movement, size changes and reversed insertion order.
			std::vector<Entry> frame = entries;
			if (round == 1)
			{
				frame.front().min = glm::vec3(100);
				frame.front().max = glm::vec3(101);
				frame[2].min = glm::vec3(100);
				frame[2].max = glm::vec3(120);
			}
			if (round == 2)
				std::reverse(frame.begin(), frame.end());
			for (const auto& entry : frame)
				grid.InsertAABB(entry.entity, entry.min, entry.max);

			size_t references = 0;
			for (const auto& cell : grid.cells)
				references += cell.second.size();
			if (references > frame.size() * 64)
			{
				std::cerr << "ArenaBroadphaseSmokeTest: grid insertion budget exceeded" << std::endl;
				return false;
			}

			// Duplicate insertion must not emit self-pairs or repeat callbacks.
			grid.InsertAABB(frame.front().entity, frame.front().min, frame.front().max);
			using Pair = std::pair<unsigned, unsigned>;
			std::set<Pair> candidates;
			bool unique = true;
			grid.ForEachCandidatePair([&](const auto& a, const auto& b)
			{
				const Pair pair(std::min(a->GetID(), b->GetID()), std::max(a->GetID(), b->GetID()));
				if (a == b || !candidates.insert(pair).second)
					unique = false;
			});
			if (!unique)
				return false;

			// The broadphase may return extra candidates, but cannot miss any brute-force AABB overlap.
			for (size_t i = 0; i < frame.size(); ++i)
			{
				for (size_t j = i + 1; j < frame.size(); ++j)
				{
					const auto& a = frame[i];
					const auto& b = frame[j];
					bool disjoint = false;
					for (int axis = 0; axis < 3; ++axis)
						disjoint = disjoint || a.max[axis] < b.min[axis] || b.max[axis] < a.min[axis];
					const Pair pair(std::min(a.entity->GetID(), b.entity->GetID()), std::max(a.entity->GetID(), b.entity->GetID()));
					if (!disjoint && candidates.count(pair) != 1)
					{
						std::cerr << "ArenaBroadphaseSmokeTest: missed overlapping pair" << std::endl;
						return false;
					}
				}
			}
		}
	}

	grid.Clear();
	auto transient = scene.CreateEntity("TransientLargeCollider");
	std::weak_ptr<Entity> lifetime = transient;
	grid.InsertAABB(transient, glm::vec3(-32), glm::vec3(32));
	transient.reset();
	scene.Clear();
	grid.Clear();
	return lifetime.expired();
}
