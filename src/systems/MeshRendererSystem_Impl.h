#pragma once

#include "rendering/ShadowMap.h"
#include "rendering/PointShadowMap.h"
#include <array>
#include <memory>

struct MeshRendererSystem::Impl
{
	Impl()
	{
	}

	~Impl() = default;

	MyEngine::ShadowMap shadowMap;
	unsigned int shadowSize = 2048;
	float shadowBias = 0.005f;
	bool shadowsEnabled = true;

	std::array<MyEngine::PointShadowMap, 4> pointShadowMaps;
	unsigned int pointShadowSize = 1024;
	float pointShadowBias = 0.02f;
	bool pointShadowsEnabled = true;

	bool wireframe = false;
};
