#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "components/TerrainComponent.h"

namespace MyEngine { class Shader; }
class Scene;

class TerrainSystem
{
public:
	struct TerrainPatchVertex
	{
		glm::vec3 position{ 0.0f };
		glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
		glm::vec2 texCoords{ 0.0f };
		glm::vec3 color{ 1.0f };
	};

	struct TerrainMeshPatchData
	{
		int resolution = 0;
		int minRow = 0;
		int maxRow = 0;
		int minCol = 0;
		int maxCol = 0;
		std::vector<TerrainPatchVertex> vertices;
	};
	// Call once after the GL context is ready. Loads the terrain shader.
	void Init();

	// Update + render all TerrainComponents in the scene.
	// view / projection come from the active camera.
	// defaultShader is used if a terrain has no shader of its own.
	void Render(Scene& scene,
				const glm::mat4& view,
				const glm::mat4& projection,
				const glm::vec3& viewPos,
				std::shared_ptr<MyEngine::Shader> defaultShader);

	// Rebuild the mesh for a terrain (call after changing settings).
	// Also called internally when dirty == true.
	static void RebuildMesh(struct TerrainComponent& terrain);

	// Sample the interpolated height at a world-space (x,z) position.
	// Returns 0 if no terrain is at that point or the component has no data.
	static float SampleHeight(const struct TerrainComponent& terrain,
							  float worldX, float worldZ,
							  const glm::vec3& terrainWorldPosition = glm::vec3(0.0f));

	// Estimate world-space terrain normal at (x,z).
	static glm::vec3 SampleNormal(const struct TerrainComponent& terrain,
							  float worldX, float worldZ,
							  const glm::vec3& terrainWorldPosition = glm::vec3(0.0f));

	// Raycast a heightfield terrain in world space.
	static bool RaycastTerrain(const struct TerrainComponent& terrain,
							  const glm::vec3& terrainWorldPosition,
							  const glm::vec3& rayOrigin,
							  const glm::vec3& rayDirection,
							  glm::vec3& outHitPoint,
							  float maxDistance = 1000.0f);

	// Apply a sculpt brush in world space.
	// RaiseLower: positive delta raises, negative lowers.
	// Smooth: blends toward local neighborhood average.
	// Flatten: blends toward flattenHeight world-space Y.
	// Optionally returns the modified vertex patch bounds [row/col].
	static bool ApplySculptBrush(struct TerrainComponent& terrain,
							  const glm::vec3& terrainWorldPosition,
							  float worldX, float worldZ,
							  float radius,
							  float strength,
							  float falloff,
							  float deltaTime,
							  bool raise,
							  TerrainBrushMode brushMode = TerrainBrushMode::RaiseLower,
							  float flattenHeight = 0.0f,
							  int* outMinRow = nullptr,
							  int* outMaxRow = nullptr,
							  int* outMinCol = nullptr,
							  int* outMaxCol = nullptr);

	// Apply a paint brush in world space to a target material layer [0..3].
	// Blends weights toward target layer and returns modified paint texel bounds [row/col].
	static bool ApplyPaintBrush(struct TerrainComponent& terrain,
							 const glm::vec3& terrainWorldPosition,
							 float worldX, float worldZ,
							 int targetLayer,
							 float radius,
							 float strength,
							 float falloff,
							 float deltaTime,
							 int* outMinRow = nullptr,
							 int* outMaxRow = nullptr,
							 int* outMinCol = nullptr,
							 int* outMaxCol = nullptr);

	// Prepare a local mesh patch (CPU only) with 1-ring neighbors included.
	static bool PrepareMeshPatchData(const struct TerrainComponent& terrain,
								 int minRow,
								 int maxRow,
								 int minCol,
								 int maxCol,
								 TerrainMeshPatchData& outPatch);

	// Upload a precomputed patch to the terrain mesh (main/render thread).
	static bool ApplyMeshPatchData(struct TerrainComponent& terrain,
							   const TerrainMeshPatchData& patch);

	// Recompute and upload only a local vertex patch plus 1-ring neighbors.
	static bool RebuildMeshPatch(struct TerrainComponent& terrain,
							  int minRow,
							  int maxRow,
							  int minCol,
							  int maxCol);

	// Import height samples from an image file. Supports 8-bit and 16-bit grayscale PNG.
	// Returns false on load/format failure.
	static bool ImportHeightmap(struct TerrainComponent& terrain,
							 const std::string& filePath,
							 bool prefer16Bit = true);

	// Export height samples to grayscale PNG.
	// 16-bit uses full [0..65535], 8-bit uses [0..255].
	static bool ExportHeightmap(const struct TerrainComponent& terrain,
							 const std::string& filePath,
							 bool export16Bit);

private:
	std::shared_ptr<MyEngine::Shader> m_TerrainShader;
};
