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
#include <limits>
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
								   float worldX, float worldZ,
								   const glm::vec3& terrainWorldPosition)
{
	if (terrain.heightData.empty() || terrain.resolution < 2)
		return 0.0f;

	int res = terrain.resolution;
	const float localX = worldX - terrainWorldPosition.x;
	const float localZ = worldZ - terrainWorldPosition.z;
	float u = (localX / terrain.width) + 0.5f;
	float v = (localZ / terrain.depth) + 0.5f;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
		return terrainWorldPosition.y;

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
	return terrainWorldPosition.y + h * terrain.heightScale;
}

glm::vec3 TerrainSystem::SampleNormal(const TerrainComponent& terrain,
								   float worldX, float worldZ,
								   const glm::vec3& terrainWorldPosition)
{
	if (terrain.heightData.empty() || terrain.resolution < 2)
		return glm::vec3(0.0f, 1.0f, 0.0f);

	const float stepX = std::max(terrain.width / static_cast<float>(terrain.resolution - 1), 0.001f);
	const float stepZ = std::max(terrain.depth / static_cast<float>(terrain.resolution - 1), 0.001f);
	const float hL = SampleHeight(terrain, worldX - stepX, worldZ, terrainWorldPosition);
	const float hR = SampleHeight(terrain, worldX + stepX, worldZ, terrainWorldPosition);
	const float hD = SampleHeight(terrain, worldX, worldZ - stepZ, terrainWorldPosition);
	const float hU = SampleHeight(terrain, worldX, worldZ + stepZ, terrainWorldPosition);
	glm::vec3 n = glm::normalize(glm::vec3(hL - hR, 2.0f * std::max(stepX, stepZ), hD - hU));
	if (glm::length(n) < 0.0001f)
		return glm::vec3(0.0f, 1.0f, 0.0f);
	return n;
}

bool TerrainSystem::RaycastTerrain(const TerrainComponent& terrain,
								 const glm::vec3& terrainWorldPosition,
								 const glm::vec3& rayOrigin,
								 const glm::vec3& rayDirection,
								 glm::vec3& outHitPoint,
								 float maxDistance)
{
	if (terrain.resolution < 2 || terrain.heightData.empty())
		return false;
	if (glm::length(rayDirection) < 0.0001f)
		return false;

	const glm::vec3 dir = glm::normalize(rayDirection);
	const float halfW = terrain.width * 0.5f;
	const float halfD = terrain.depth * 0.5f;
	const float minY = terrainWorldPosition.y - 1.0f;
	const float maxY = terrainWorldPosition.y + terrain.heightScale + 1.0f;

	float tMin = 0.0f;
	float tMax = maxDistance;
	auto axisSlab = [&](float origin, float direction, float minB, float maxB) -> bool
	{
		if (std::abs(direction) < 1e-6f)
			return origin >= minB && origin <= maxB;
		float invDir = 1.0f / direction;
		float t1 = (minB - origin) * invDir;
		float t2 = (maxB - origin) * invDir;
		if (t1 > t2)
			std::swap(t1, t2);
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		return tMin <= tMax;
	};

	if (!axisSlab(rayOrigin.x, dir.x, terrainWorldPosition.x - halfW, terrainWorldPosition.x + halfW) ||
		!axisSlab(rayOrigin.y, dir.y, minY, maxY) ||
		!axisSlab(rayOrigin.z, dir.z, terrainWorldPosition.z - halfD, terrainWorldPosition.z + halfD))
	{
		return false;
	}

	const float terrainStep = std::min(
		std::max(terrain.width / static_cast<float>(std::max(terrain.resolution - 1, 1)), 0.05f),
		std::max(terrain.depth / static_cast<float>(std::max(terrain.resolution - 1, 1)), 0.05f));
	const float step = std::clamp(terrainStep * 0.5f, 0.02f, 1.0f);

	float t = std::max(tMin, 0.0f);
	float prevT = t;
	float prevDiff = std::numeric_limits<float>::max();
	bool hadPrev = false;
	while (t <= tMax)
	{
		const glm::vec3 p = rayOrigin + dir * t;
		const float terrainY = SampleHeight(terrain, p.x, p.z, terrainWorldPosition);
		const float diff = p.y - terrainY;

		if (std::abs(diff) <= 0.03f)
		{
			outHitPoint = glm::vec3(p.x, terrainY, p.z);
			return true;
		}

		if (hadPrev && ((prevDiff > 0.0f && diff < 0.0f) || (prevDiff < 0.0f && diff > 0.0f)))
		{
			float a = prevT;
			float b = t;
			for (int i = 0; i < 8; ++i)
			{
				const float mid = 0.5f * (a + b);
				const glm::vec3 mp = rayOrigin + dir * mid;
				const float my = SampleHeight(terrain, mp.x, mp.z, terrainWorldPosition);
				const float mdiff = mp.y - my;
				if ((prevDiff > 0.0f && mdiff > 0.0f) || (prevDiff < 0.0f && mdiff < 0.0f))
					a = mid;
				else
					b = mid;
			}
			const float hitT = 0.5f * (a + b);
			const glm::vec3 hp = rayOrigin + dir * hitT;
			outHitPoint = glm::vec3(hp.x, SampleHeight(terrain, hp.x, hp.z, terrainWorldPosition), hp.z);
			return true;
		}

		hadPrev = true;
		prevT = t;
		prevDiff = diff;
		t += step;
	}

	return false;
}

bool TerrainSystem::ApplySculptBrush(TerrainComponent& terrain,
								  const glm::vec3& terrainWorldPosition,
								  float worldX, float worldZ,
								  float radius,
								  float strength,
								  float falloff,
								  float deltaTime,
								  bool raise,
								  TerrainBrushMode brushMode,
								  float flattenHeight,
								  int* outMinRow,
								  int* outMaxRow,
								  int* outMinCol,
								  int* outMaxCol)
{
	if (terrain.resolution < 2)
		return false;

	const int res = std::clamp(terrain.resolution, 2, 512);
	if (terrain.heightData.size() != static_cast<size_t>(res) * static_cast<size_t>(res))
		terrain.heightData.assign(static_cast<size_t>(res) * static_cast<size_t>(res), 0.0f);

	const float brushRadius = std::max(radius, 0.01f);
	const float brushStrength = std::max(strength, 0.0f);
	const float brushFalloff = std::max(falloff, 0.01f);
	const float sign = raise ? 1.0f : -1.0f;

	const float localX = worldX - terrainWorldPosition.x;
	const float localZ = worldZ - terrainWorldPosition.z;
	const float halfW = terrain.width * 0.5f;
	const float halfD = terrain.depth * 0.5f;
	if (localX < -halfW || localX > halfW || localZ < -halfD || localZ > halfD)
		return false;

	std::vector<float> original = terrain.heightData;
	bool changed = false;
	int minRow = res - 1;
	int maxRow = 0;
	int minCol = res - 1;
	int maxCol = 0;

	auto sampleOriginal = [&](int r, int c) -> float
	{
		r = std::clamp(r, 0, res - 1);
		c = std::clamp(c, 0, res - 1);
		return original[static_cast<size_t>(r) * static_cast<size_t>(res) + static_cast<size_t>(c)];
	};

	const float targetFlattenSample = terrain.heightScale > 0.0001f
		? std::clamp((flattenHeight - terrainWorldPosition.y) / terrain.heightScale, 0.0f, 1.0f)
		: 0.0f;

	for (int row = 0; row < res; ++row)
	{
		const float pz = (static_cast<float>(row) / static_cast<float>(res - 1) - 0.5f) * terrain.depth;
		for (int col = 0; col < res; ++col)
		{
			const float px = (static_cast<float>(col) / static_cast<float>(res - 1) - 0.5f) * terrain.width;
			const float dx = px - localX;
			const float dz = pz - localZ;
			const float dist = std::sqrt(dx * dx + dz * dz);
			if (dist > brushRadius)
				continue;

			const float t = 1.0f - (dist / brushRadius);
			const float influence = std::pow(std::clamp(t, 0.0f, 1.0f), brushFalloff);
			float& sample = terrain.heightData[static_cast<size_t>(row) * static_cast<size_t>(res) + static_cast<size_t>(col)];
			const float current = original[static_cast<size_t>(row) * static_cast<size_t>(res) + static_cast<size_t>(col)];
			float updated = current;

			switch (brushMode)
			{
			case TerrainBrushMode::Smooth:
			{
				const float avg = (
					sampleOriginal(row - 1, col - 1) + sampleOriginal(row - 1, col) + sampleOriginal(row - 1, col + 1) +
					sampleOriginal(row, col - 1) + current + sampleOriginal(row, col + 1) +
					sampleOriginal(row + 1, col - 1) + sampleOriginal(row + 1, col) + sampleOriginal(row + 1, col + 1)) / 9.0f;
				const float blend = std::clamp(influence * brushStrength * deltaTime, 0.0f, 1.0f);
				updated = current + (avg - current) * blend;
				break;
			}
			case TerrainBrushMode::Flatten:
			{
				const float blend = std::clamp(influence * brushStrength * deltaTime, 0.0f, 1.0f);
				updated = current + (targetFlattenSample - current) * blend;
				break;
			}
			case TerrainBrushMode::RaiseLower:
			default:
				updated = current + sign * influence * brushStrength * deltaTime;
				break;
			}

			updated = std::clamp(updated, 0.0f, 1.0f);
			if (std::abs(updated - sample) < 1e-6f)
				continue;
			sample = updated;
			changed = true;
			minRow = std::min(minRow, row);
			maxRow = std::max(maxRow, row);
			minCol = std::min(minCol, col);
			maxCol = std::max(maxCol, col);
		}
	}

	if (!changed)
		return false;

	if (outMinRow) *outMinRow = minRow;
	if (outMaxRow) *outMaxRow = maxRow;
	if (outMinCol) *outMinCol = minCol;
	if (outMaxCol) *outMaxCol = maxCol;
	terrain.dirty = true;
	return true;
}

bool TerrainSystem::PrepareMeshPatchData(const TerrainComponent& terrain,
								 int minRow,
								 int maxRow,
								 int minCol,
								 int maxCol,
								 TerrainMeshPatchData& outPatch)
{
	if (terrain.resolution < 2 || terrain.heightData.empty())
		return false;

	const int res = std::clamp(terrain.resolution, 2, 512);
	if (terrain.heightData.size() != static_cast<size_t>(res) * static_cast<size_t>(res))
		return false;

	outPatch.resolution = res;
	outPatch.minRow = std::clamp(minRow - 1, 0, res - 1);
	outPatch.maxRow = std::clamp(maxRow + 1, 0, res - 1);
	outPatch.minCol = std::clamp(minCol - 1, 0, res - 1);
	outPatch.maxCol = std::clamp(maxCol + 1, 0, res - 1);

	const int patchRows = outPatch.maxRow - outPatch.minRow + 1;
	const int patchCols = outPatch.maxCol - outPatch.minCol + 1;
	if (patchRows <= 0 || patchCols <= 0)
		return false;

	outPatch.vertices.clear();
	outPatch.vertices.reserve(static_cast<size_t>(patchRows) * static_cast<size_t>(patchCols));

	auto sampleH = [&](int r, int c) -> float
	{
		r = std::clamp(r, 0, res - 1);
		c = std::clamp(c, 0, res - 1);
		return terrain.heightData[static_cast<size_t>(r) * static_cast<size_t>(res) + static_cast<size_t>(c)] * terrain.heightScale;
	};

	for (int row = outPatch.minRow; row <= outPatch.maxRow; ++row)
	{
		for (int col = outPatch.minCol; col <= outPatch.maxCol; ++col)
		{
			const float u = col / static_cast<float>(res - 1);
			const float v = row / static_cast<float>(res - 1);
			const float px = (u - 0.5f) * terrain.width;
			const float pz = (v - 0.5f) * terrain.depth;
			const float py = terrain.heightData[static_cast<size_t>(row) * static_cast<size_t>(res) + static_cast<size_t>(col)] * terrain.heightScale;

			TerrainPatchVertex vert;
			vert.position = { px, py, pz };
			vert.texCoords = { u, v };
			vert.color = { 1.0f, 1.0f, 1.0f };

			const float dx = sampleH(row, col + 1) - sampleH(row, col - 1);
			const float dz = sampleH(row + 1, col) - sampleH(row - 1, col);
			const float stepX = terrain.width / static_cast<float>(res - 1);
			const float stepZ = terrain.depth / static_cast<float>(res - 1);
			vert.normal = glm::normalize(glm::vec3(-dx / (2.0f * stepX), 1.0f, -dz / (2.0f * stepZ)));
			outPatch.vertices.push_back(vert);
		}
	}

	return true;
}

bool TerrainSystem::ApplyMeshPatchData(TerrainComponent& terrain,
							   const TerrainMeshPatchData& patch)
{
	if (!terrain.mesh || patch.resolution < 2 || patch.vertices.empty())
		return false;

	const int res = std::clamp(terrain.resolution, 2, 512);
	if (res != patch.resolution)
		return false;

	const int minRow = std::clamp(patch.minRow, 0, res - 1);
	const int maxRow = std::clamp(patch.maxRow, 0, res - 1);
	const int minCol = std::clamp(patch.minCol, 0, res - 1);
	const int maxCol = std::clamp(patch.maxCol, 0, res - 1);
	if (maxRow < minRow || maxCol < minCol)
		return false;

	const int patchCols = maxCol - minCol + 1;
	const size_t expectedVertexCount = static_cast<size_t>(maxRow - minRow + 1) * static_cast<size_t>(patchCols);
	if (patch.vertices.size() != expectedVertexCount)
		return false;

	for (int row = minRow; row <= maxRow; ++row)
	{
		const size_t rowIndex = static_cast<size_t>(row - minRow) * static_cast<size_t>(patchCols);
		std::vector<MyEngine::Vertex> rowVertices;
		rowVertices.reserve(static_cast<size_t>(patchCols));
		for (int c = 0; c < patchCols; ++c)
		{
			const auto& p = patch.vertices[rowIndex + static_cast<size_t>(c)];
			MyEngine::Vertex v;
			v.Position = p.position;
			v.Normal = p.normal;
			v.TexCoords = p.texCoords;
			v.Color = p.color;
			rowVertices.push_back(v);
		}
		const size_t firstVertex = static_cast<size_t>(row) * static_cast<size_t>(res) + static_cast<size_t>(minCol);
		if (!terrain.mesh->UpdateVertexRange(firstVertex, static_cast<size_t>(patchCols), rowVertices.data()))
			return false;
	}

	terrain.dirty = false;
	return true;
}

bool TerrainSystem::RebuildMeshPatch(TerrainComponent& terrain,
							  int minRow,
							  int maxRow,
							  int minCol,
							  int maxCol)
{
	if (!terrain.mesh)
		return false;

	TerrainMeshPatchData patch;
	if (!PrepareMeshPatchData(terrain, minRow, maxRow, minCol, maxCol, patch))
		return false;

	return ApplyMeshPatchData(terrain, patch);
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
