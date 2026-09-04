#pragma once

#include "rendering/ShadowMap.h"
#include "rendering/PointShadowMap.h"
#include "rendering/SSAOPass.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <glad/glad.h>

static constexpr int MAX_CASCADES = 4;

struct MeshRendererSystem::Impl
{
	Impl()
	{
	}

	~Impl() = default;

	// Cascaded shadow maps (one FBO+texture per cascade)
	std::array<MyEngine::ShadowMap, MAX_CASCADES> cascadeMaps;
	unsigned int shadowSize   = 2048;
	float        shadowBias   = 0.001f;
	bool         shadowsEnabled = true;

	// CSM settings
	int   numCascades  = 4;      // active cascade count (1-4)
	float splitLambda  = 0.99f;  // blend between log (1.0) and uniform (0.0) splits
	float cascadeBlendFactor = 0.15f;

	std::array<MyEngine::PointShadowMap, 4> pointShadowMaps;
	unsigned int pointShadowSize = 1024;
	// Global multiplier anchor for point-light shadow bias. 0.005f is neutral
	// (matches LightComponent::shadowBias default).
	float pointShadowBias = 0.005f;
	bool pointShadowsEnabled = true;
	int pointShadowPCFSamples = 20;
	float pointShadowPCFRadius = 0.02f;
	int pointShadowLightBudget = 4;

	// Spot light shadow maps (one 2D depth map per shadow-casting spot light)
	std::array<MyEngine::ShadowMap, 4> spotShadowMaps;
	unsigned int spotShadowSize = 1024;
	bool spotShadowsEnabled = true;
	float spotShadowPCFRadius = 1.0f;
	int spotShadowLightBudget = 4;
	bool shadowStabilizationEnabled = true;

	MeshRendererSystem::ShadowDiagnostics shadowDiagnostics;
	MeshRendererSystem::OcclusionDiagnostics occlusionDiagnostics;
	bool occlusionApproximationEnabled = false;
	bool gpuOcclusionQueriesEnabled = false;
	int occlusionQueryRecheckFrames = 6;
	std::unordered_map<uint32_t, int> occlusionVisibilityFrames;
	std::unordered_map<uint32_t, bool> occlusionLastVisible;
	MeshRendererSystem::DebugViewMode debugViewMode = MeshRendererSystem::DebugViewMode::FinalLit;
	bool wireframe = false;

	// Skinning palette UBO path (with uniform-array fallback per shader).
	GLuint skinningPaletteUBO = 0;
	GLuint skinningPaletteBindingPoint = 7;

	// SSAO
	MyEngine::SSAOPass ssaoPass;
	bool  ssaoEnabled = true;

	// IBL
	bool         iblEnabled        = false;
	float        iblIntensity      = 1.0f;
	unsigned int pendingIBLCubemap = 0;  // set by InitIBL; baked on next Render()
};
