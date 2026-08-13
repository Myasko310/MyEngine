#pragma once

#include "rendering/ShadowMap.h"
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
	bool wireframe = false;
};
