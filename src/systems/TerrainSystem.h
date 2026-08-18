#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace MyEngine { class Shader; }
class Scene;

class TerrainSystem
{
public:
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
							  float worldX, float worldZ);

private:
	std::shared_ptr<MyEngine::Shader> m_TerrainShader;
};
