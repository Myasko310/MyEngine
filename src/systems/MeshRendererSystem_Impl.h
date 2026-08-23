#pragma once

#include "rendering/ShadowMap.h"
#include "rendering/PointShadowMap.h"
#include "rendering/SSAOPass.h"
#include <array>
#include <memory>

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

	std::array<MyEngine::PointShadowMap, 4> pointShadowMaps;
	unsigned int pointShadowSize = 1024;
	// Global multiplier anchor for point-light shadow bias. 0.005f is neutral
	// (matches LightComponent::shadowBias default).
	float pointShadowBias = 0.005f;
	bool pointShadowsEnabled = true;

	bool wireframe = false;

	// SSAO
	MyEngine::SSAOPass ssaoPass;
	bool  ssaoEnabled = true;

	// IBL
	bool         iblEnabled        = false;
	float        iblIntensity      = 1.0f;
	unsigned int pendingIBLCubemap = 0;  // set by InitIBL; baked on next Render()
};
