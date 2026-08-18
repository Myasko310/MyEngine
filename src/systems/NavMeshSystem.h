#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Scene;

class NavMeshSystem
{
public:
	NavMeshSystem() = default;

	// Bake a navigation grid from the scene's BoxCollider/BoundingSphere
	// components.
	// origin      – world-space bottom-left corner of the grid
	// size        – world-space extent (width / depth)
	// cellSize    – size of each grid cell
	// agentHeight – minimum clearance needed (used to mark cells blocked)
	void Bake(Scene& scene,
			  glm::vec2 origin    = { -50.0f, -50.0f },
			  glm::vec2 size      = {  100.0f, 100.0f },
			  float     cellSize  = 1.0f,
			  float     agentHeight = 1.8f);

	// Find a path from `start` to `goal` using A*.
	// Returns an ordered list of world-space waypoints (including `goal`),
	// or an empty vector if no path exists or the grid has not been baked.
	std::vector<glm::vec3> FindPath(glm::vec3 start, glm::vec3 goal) const;

	// Move all NavigationAgentComponents toward their targets.
	// Call once per frame with the elapsed time.
	void Update(Scene& scene, float deltaTime);

	// Returns true if the grid has been baked at least once.
	bool IsReady() const { return m_Ready; }

	// Grid accessor for debug/visualisation.
	bool IsCellWalkable(int col, int row) const;
	int  GetCols()     const { return m_Cols; }
	int  GetRows()     const { return m_Rows; }
	glm::vec2 GetOrigin()   const { return m_Origin; }
	float     GetCellSize() const { return m_CellSize; }

private:
	// Convert world position to grid cell (col, row).
	bool WorldToCell(glm::vec3 pos, int& col, int& row) const;
	// Convert grid cell to world-space centre position.
	glm::vec3 CellToWorld(int col, int row) const;

	std::vector<bool> m_Grid;  // true = walkable
	int       m_Cols     = 0;
	int       m_Rows     = 0;
	glm::vec2 m_Origin   = { 0, 0 };
	float     m_CellSize = 1.0f;
	float     m_AgentY   = 0.0f;  // Y level of the navigation plane
	bool      m_Ready    = false;
};
