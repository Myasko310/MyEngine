#include "systems/NavMeshSystem.h"

#include "components/NavigationAgentComponent.h"
#include "components/TransformComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/BoundingSphereComponent.h"
#include "ecs/Scene.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Bake
// ---------------------------------------------------------------------------

void NavMeshSystem::Bake(Scene& scene,
						  glm::vec2 origin,
						  glm::vec2 size,
						  float     cellSize,
						  float     /*agentHeight*/)
{
	m_Origin   = origin;
	m_CellSize = cellSize;
	m_Cols     = static_cast<int>(std::ceil(size.x / cellSize));
	m_Rows     = static_cast<int>(std::ceil(size.y / cellSize));

	// Start with every cell walkable.
	m_Grid.assign(static_cast<size_t>(m_Cols) * m_Rows, true);

	// Mark cells occupied by scene colliders as blocked.
	for (auto& entity : scene.GetEntities())
	{
		if (!entity) continue;

		if (entity->HasComponent<BoxColliderComponent>() &&
			entity->HasComponent<TransformComponent>())
		{
			auto& tc  = entity->GetComponent<TransformComponent>();
			auto& box = entity->GetComponent<BoxColliderComponent>();

			// World-space AABB of the box
			glm::vec3 worldCenter = tc.position + box.center;
			glm::vec3 half        = box.halfExtents;

			// XZ footprint
			float minX = worldCenter.x - half.x;
			float maxX = worldCenter.x + half.x;
			float minZ = worldCenter.z - half.z;
			float maxZ = worldCenter.z + half.z;

			int c0 = static_cast<int>((minX - m_Origin.x) / m_CellSize);
			int c1 = static_cast<int>((maxX - m_Origin.x) / m_CellSize);
			int r0 = static_cast<int>((minZ - m_Origin.y) / m_CellSize);
			int r1 = static_cast<int>((maxZ - m_Origin.y) / m_CellSize);

			for (int r = r0; r <= r1; ++r)
				for (int c = c0; c <= c1; ++c)
					if (c >= 0 && c < m_Cols && r >= 0 && r < m_Rows)
						m_Grid[r * m_Cols + c] = false;
		}
		else if (entity->HasComponent<BoundingSphereComponent>() &&
				 entity->HasComponent<TransformComponent>())
		{
			auto& tc = entity->GetComponent<TransformComponent>();
			auto& bs = entity->GetComponent<BoundingSphereComponent>();

			float cx = tc.position.x + bs.center.x;
			float cz = tc.position.z + bs.center.z;
			float r  = bs.radius;

			int c0 = static_cast<int>((cx - r - m_Origin.x) / m_CellSize);
			int c1 = static_cast<int>((cx + r - m_Origin.x) / m_CellSize);
			int r0 = static_cast<int>((cz - r - m_Origin.y) / m_CellSize);
			int r1 = static_cast<int>((cz + r - m_Origin.y) / m_CellSize);

			for (int row = r0; row <= r1; ++row)
				for (int col = c0; col <= c1; ++col)
				{
					if (col < 0 || col >= m_Cols || row < 0 || row >= m_Rows)
						continue;
					float wx = m_Origin.x + (col + 0.5f) * m_CellSize;
					float wz = m_Origin.y + (row + 0.5f) * m_CellSize;
					float dx = wx - cx, dz = wz - cz;
					if (dx * dx + dz * dz <= r * r)
						m_Grid[row * m_Cols + col] = false;
				}
		}
	}

	m_Ready = true;
}

// ---------------------------------------------------------------------------
// World / Cell conversion
// ---------------------------------------------------------------------------

bool NavMeshSystem::WorldToCell(glm::vec3 pos, int& col, int& row) const
{
	col = static_cast<int>((pos.x - m_Origin.x) / m_CellSize);
	row = static_cast<int>((pos.z - m_Origin.y) / m_CellSize);
	return col >= 0 && col < m_Cols && row >= 0 && row < m_Rows;
}

glm::vec3 NavMeshSystem::CellToWorld(int col, int row) const
{
	return {
		m_Origin.x + (col + 0.5f) * m_CellSize,
		m_AgentY,
		m_Origin.y + (row + 0.5f) * m_CellSize
	};
}

bool NavMeshSystem::IsCellWalkable(int col, int row) const
{
	if (col < 0 || col >= m_Cols || row < 0 || row >= m_Rows) return false;
	return m_Grid[row * m_Cols + col];
}

// ---------------------------------------------------------------------------
// A* pathfinding
// ---------------------------------------------------------------------------

std::vector<glm::vec3> NavMeshSystem::FindPath(glm::vec3 start,
												 glm::vec3 goal) const
{
	if (!m_Ready) return {};

	int sc, sr, gc, gr;
	if (!WorldToCell(start, sc, sr)) return {};
	if (!WorldToCell(goal,  gc, gr)) return {};

	if (!IsCellWalkable(sc, sr) || !IsCellWalkable(gc, gr)) return {};
	if (sc == gc && sr == gr)
		return { goal };

	struct Node { int c, r; float g, f; };
	auto cmp = [](const Node& a, const Node& b){ return a.f > b.f; };
	std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);

	auto idx = [&](int c, int r){ return r * m_Cols + c; };
	auto heur = [&](int c, int r) -> float {
		float dc = static_cast<float>(c - gc);
		float dr = static_cast<float>(r - gr);
		return std::sqrt(dc * dc + dr * dr);
	};

	std::vector<float>    gScore(static_cast<size_t>(m_Cols) * m_Rows, 1e30f);
	std::vector<int>      from  (static_cast<size_t>(m_Cols) * m_Rows, -1);
	std::vector<bool>     closed(static_cast<size_t>(m_Cols) * m_Rows, false);

	gScore[idx(sc, sr)] = 0.0f;
	open.push({ sc, sr, 0.0f, heur(sc, sr) });

	const int dc[] = { 0, 0,  1, -1,  1,  1, -1, -1 };
	const int dr[] = { 1,-1,  0,  0,  1, -1,  1, -1 };
	const float cost[] = { 1, 1, 1, 1, 1.414f, 1.414f, 1.414f, 1.414f };

	bool found = false;
	while (!open.empty())
	{
		auto cur = open.top(); open.pop();
		if (closed[idx(cur.c, cur.r)]) continue;
		closed[idx(cur.c, cur.r)] = true;

		if (cur.c == gc && cur.r == gr) { found = true; break; }

		for (int d = 0; d < 8; ++d)
		{
			int nc = cur.c + dc[d], nr = cur.r + dr[d];
			if (!IsCellWalkable(nc, nr)) continue;
			if (closed[idx(nc, nr)])    continue;

			float ng = gScore[idx(cur.c, cur.r)] + cost[d];
			if (ng < gScore[idx(nc, nr)])
			{
				gScore[idx(nc, nr)] = ng;
				from  [idx(nc, nr)] = idx(cur.c, cur.r);
				open.push({ nc, nr, ng, ng + heur(nc, nr) });
			}
		}
	}

	if (!found) return {};

	// Reconstruct path
	std::vector<glm::vec3> path;
	int cur = idx(gc, gr);
	while (cur != -1)
	{
		int c = cur % m_Cols, r = cur / m_Cols;
		path.push_back(CellToWorld(c, r));
		cur = from[cur];
	}
	// Replace last waypoint with exact goal position and reverse
	if (!path.empty()) path.front() = goal;
	std::reverse(path.begin(), path.end());
	return path;
}

// ---------------------------------------------------------------------------
// Update agents
// ---------------------------------------------------------------------------

void NavMeshSystem::Update(Scene& scene, float deltaTime)
{
	for (auto& entity : scene.GetEntities())
	{
		if (!entity) continue;
		if (!entity->HasComponent<NavigationAgentComponent>()) continue;
		if (!entity->HasComponent<TransformComponent>())       continue;

		auto& agent     = entity->GetComponent<NavigationAgentComponent>();
		auto& transform = entity->GetComponent<TransformComponent>();

		if (!agent.active || agent.arrived || agent.path.empty())
			continue;

		// Pursue current waypoint
		glm::vec3 wp    = agent.path[agent.waypointIndex];
		glm::vec3 toWp  = wp - transform.position;
		toWp.y = 0.0f;  // move on the XZ plane only
		float dist = glm::length(toWp);

		if (dist <= agent.stoppingDistance)
		{
			++agent.waypointIndex;
			if (agent.waypointIndex >= static_cast<int>(agent.path.size()))
			{
				agent.arrived = true;
				agent.active  = false;
			}
			continue;
		}

		glm::vec3 dir = toWp / dist;
		transform.position += dir * agent.speed * deltaTime;

		// Face the direction of travel (Y-axis rotation)
		transform.rotation.y = glm::degrees(std::atan2(dir.x, dir.z));
	}
}
