#pragma once

#include "ecs/Scene.h"
#include <string>

namespace MyEngine
{
	namespace Serialization
	{
		bool SaveScene(const ::Scene& scene, const std::string& path);
		bool LoadScene(::Scene& scene, const std::string& path);
	}
}
