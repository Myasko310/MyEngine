#include "systems/TerrainSystem.h"

#include "components/TerrainComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"
#include "core/AssetManager.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// stb_image for greyscale heightmap loading
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Mesh generation
// ---------------------------------------------------------------------------

void TerrainSystem::RebuildMesh(TerrainComponent& terrain)
{
	int res = std::clamp(terrain.resolution, 2, 512);
	terrain.resolution = res;

	// Build height data from PNG if a path is supplied
	if (!terrain.heightmapPath.empty())
	{
		int w, h, ch;
		// Load as single-channel (greyscale)
		unsigned char* data = stbi_load(terrain.heightmapPath.c_str(), &w, &h, &ch, 1);
		if (data)
		{
			terrain.heightData.resize(static_cast<size_t>(res) * res);
			for (int row = 0; row < res; ++row)
			{
				for (int col = 0; col < res; ++col)
				{
					// Map grid sample to image coordinates
					int px = static_cast<int>(col * (w - 1) / static_cast<float>(res - 1));
					int py = static_cast<int>(row * (h - 1) / static_cast<float>(res - 1));
					px = std::clamp(px, 0, w - 1);
					py = std::clamp(py, 0, h - 1);
					terrain.heightData[row * res + col] =
						data[py * w + px] / 255.0f;
				}
			}
			stbi_image_free(data);
		}
		else
		{
			std::cerr << "[TerrainSystem] Failed to load heightmap: "
					  << terrain.heightmapPath << "\n";
			terrain.heightData.assign(static_cast<size_t>(res) * res, 0.0f);
		}
	}
	else
	{
		// Flat terrain
		terrain.heightData.assign(static_cast<size_t>(res) * res, 0.0f);
	}

	// Generate vertices
	std::vector<MyEngine::Vertex> verts;
	verts.reserve(static_cast<size_t>(res) * res);

	for (int row = 0; row < res; ++row)
	{
		for (int col = 0; col < res; ++col)
		{
			float u = col / static_cast<float>(res - 1);
			float v = row / static_cast<float>(res - 1);

			float px = (u - 0.5f) * terrain.width;
			float pz = (v - 0.5f) * terrain.depth;
			float py = terrain.heightData[row * res + col] * terrain.heightScale;

			MyEngine::Vertex vert;
			vert.Position  = { px, py, pz };
			vert.TexCoords = { u, v };
			vert.Color     = { 1.0f, 1.0f, 1.0f };

			// Compute normal via finite differences (clamped at edges)
			auto sampleH = [&](int r, int c) -> float {
				r = std::clamp(r, 0, res - 1);
				c = std::clamp(c, 0, res - 1);
				return terrain.heightData[r * res + c] * terrain.heightScale;
			};
			float dx = sampleH(row,     col + 1) - sampleH(row,     col - 1);
			float dz = sampleH(row + 1, col    ) - sampleH(row - 1, col    );
			float stepX = terrain.width  / static_cast<float>(res - 1);
			float stepZ = terrain.depth  / static_cast<float>(res - 1);
			glm::vec3 n = glm::normalize(glm::vec3(-dx / (2.0f * stepX),
													1.0f,
												   -dz / (2.0f * stepZ)));
			vert.Normal = n;

			verts.push_back(vert);
		}
	}

	// Generate indices (two triangles per quad)
	std::vector<unsigned int> indices;
	indices.reserve(static_cast<size_t>(res - 1) * (res - 1) * 6);
	for (int row = 0; row < res - 1; ++row)
	{
		for (int col = 0; col < res - 1; ++col)
		{
			unsigned int tl = row       * res + col;
			unsigned int tr = row       * res + col + 1;
			unsigned int bl = (row + 1) * res + col;
			unsigned int br = (row + 1) * res + col + 1;
			indices.push_back(tl); indices.push_back(bl); indices.push_back(tr);
			indices.push_back(tr); indices.push_back(bl); indices.push_back(br);
		}
	}

	terrain.mesh = std::make_shared<MyEngine::Mesh>(verts, indices);
	terrain.dirty = false;
}

float TerrainSystem::SampleHeight(const TerrainComponent& terrain,
								   float worldX, float worldZ)
{
	if (terrain.heightData.empty() || terrain.resolution < 2)
		return 0.0f;

	int res = terrain.resolution;
	// Convert world-space to [0,1] UV
	float u = (worldX / terrain.width)  + 0.5f;
	float v = (worldZ / terrain.depth)  + 0.5f;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
		return 0.0f;

	float col = u * (res - 1);
	float row = v * (res - 1);

	int c0 = static_cast<int>(col), c1 = std::min(c0 + 1, res - 1);
	int r0 = static_cast<int>(row), r1 = std::min(r0 + 1, res - 1);
	float fc = col - c0, fr = row - r0;

	float h00 = terrain.heightData[r0 * res + c0];
	float h10 = terrain.heightData[r0 * res + c1];
	float h01 = terrain.heightData[r1 * res + c0];
	float h11 = terrain.heightData[r1 * res + c1];

	float h = h00 * (1 - fc) * (1 - fr)
			+ h10 * fc       * (1 - fr)
			+ h01 * (1 - fc) * fr
			+ h11 * fc       * fr;
	return h * terrain.heightScale;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void TerrainSystem::Init()
{
	// The terrain reuses the engine's default Blinn-Phong shader unless
	// the component supplies its own. We do NOT preload a shader here so
	// that the caller can supply any default it likes at render time.
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void TerrainSystem::Render(Scene& scene,
							const glm::mat4& view,
							const glm::mat4& projection,
							const glm::vec3& viewPos,
							std::shared_ptr<MyEngine::Shader> defaultShader)
{
	for (auto& entity : scene.GetEntities())
	{
		if (!entity->HasComponent<TerrainComponent>())
			continue;

		auto& terrain = entity->GetComponent<TerrainComponent>();

		// Rebuild mesh if dirty or not yet built
		if (terrain.dirty || !terrain.mesh)
			RebuildMesh(terrain);

		if (!terrain.mesh)
			continue;

		// World transform (uses TransformComponent if present, otherwise identity)
		glm::mat4 model = glm::mat4(1.0f);
		if (entity->HasComponent<TransformComponent>())
		{
			auto& tc = entity->GetComponent<TransformComponent>();
			model = glm::translate(glm::mat4(1.0f), tc.position);
			model = glm::rotate(model, glm::radians(tc.rotation.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(tc.rotation.x), glm::vec3(1, 0, 0));
			model = glm::rotate(model, glm::radians(tc.rotation.z), glm::vec3(0, 0, 1));
			model = glm::scale(model, tc.scale);
		}

		// Pick shader
		auto shader = terrain.shader ? terrain.shader : defaultShader;
		if (!terrain.shader && !terrain.shaderVertPath.empty() && !terrain.shaderFragPath.empty())
		{
			terrain.shader = MyEngine::AssetManager::LoadShader(terrain.shaderVertPath,
																 terrain.shaderFragPath);
			shader = terrain.shader;
		}
		if (!shader)
			continue;

		shader->Use();
		shader->SetMat4("u_Model",      model);
		shader->SetMat4("u_View",       view);
		shader->SetMat4("u_Projection", projection);
		shader->SetVec3("u_ViewPos",    viewPos);

		// Bind surface texture to unit 0 if present
		if (terrain.surfaceTexture)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, terrain.surfaceTexture->GetID());
			shader->SetInt("u_Texture", 0);
			shader->SetBool("u_UseTexture", true);
		}
		else
		{
			shader->SetBool("u_UseTexture", false);
		}

		terrain.mesh->Draw();
	}
}
