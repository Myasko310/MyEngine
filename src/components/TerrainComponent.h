#pragma once

#include <string>
#include <memory>
#include <vector>

namespace MyEngine { class Mesh; class Shader; class Texture; }

enum class TerrainBrushMode
{
	RaiseLower = 0,
	Smooth = 1,
	Flatten = 2
};

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

	// Manual sculpt settings
	bool sculptEnabled = false;
	float sculptBrushRadius = 3.0f;
	float sculptBrushStrength = 0.5f;
	float sculptBrushFalloff = 1.0f;
	bool sculptRaise = true;
	TerrainBrushMode sculptBrushMode = TerrainBrushMode::RaiseLower;
	float sculptFlattenHeight = 0.0f;

	// Internal: raw height samples [resolution x resolution], [0,1]
	std::vector<float> heightData;

	// Internal deferred patch update state
	bool sculptPatchDirty = false;
	int sculptPatchMinRow = 0;
	int sculptPatchMaxRow = 0;
	int sculptPatchMinCol = 0;
	int sculptPatchMaxCol = 0;
	float sculptPatchAccumulatedTime = 0.0f;
	float sculptPatchCommitInterval = 0.03f;

	// Dirty flag – set to true to trigger mesh rebuild next frame
	bool dirty = true;
};
