#pragma once

#include <string>
#include <memory>
#include <vector>

namespace MyEngine { class Mesh; class Shader; class Texture; }

struct TerrainComponent
{
	// Heightmap asset path (greyscale PNG/JPG)
	std::string heightmapPath;

	// World-space dimensions
	float width        = 100.0f;  // X extent
	float depth        = 100.0f;  // Z extent
	float heightScale  =  20.0f;  // Maximum height (Y)
	int   resolution   =  128;    // Vertices per side (clamped to [2, 512])

	// Runtime-generated mesh (rebuilt whenever settings change)
	std::shared_ptr<MyEngine::Mesh> mesh;

	// Optional textures for the terrain surface
	std::shared_ptr<MyEngine::Texture> surfaceTexture;
	std::string surfaceTexturePath;

	// Optional override shader (falls back to the scene default)
	std::shared_ptr<MyEngine::Shader> shader;
	std::string shaderVertPath;
	std::string shaderFragPath;

	// Internal: raw height samples [resolution x resolution], [0,1]
	std::vector<float> heightData;

	// Dirty flag – set to true to trigger mesh rebuild next frame
	bool dirty = true;
};
