#pragma once

#include <string>
#include <memory>
#include <vector>

namespace MyEngine { class Mesh; class Shader; class Texture; }

constexpr int kMaxTerrainPaintLayers = 4;

enum class TerrainBrushMode
{
	RaiseLower = 0,
	Smooth = 1,
	Flatten = 2
};

struct TerrainPaintLayer
{
	std::string name;
	std::string texturePath;
	std::shared_ptr<MyEngine::Texture> texture;
	float uvScale = 8.0f;
	bool enabled = true;
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

	// Optional legacy single surface texture
	std::shared_ptr<MyEngine::Texture> surfaceTexture;
	std::string surfaceTexturePath;

	// Multi-layer paint data (RGBA splat map + up to 4 material layers)
	std::vector<TerrainPaintLayer> paintLayers;
	std::vector<float> paintWeightData;
	unsigned int paintWeightTextureID = 0;
	bool paintWeightTextureDirty = true;
	int paintResolution = 128;

	// Paint brush settings
	bool paintEnabled = false;
	int paintActiveLayer = 0;
	float paintBrushRadius = 3.0f;
	float paintBrushStrength = 0.5f;
	float paintBrushFalloff = 1.0f;

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
