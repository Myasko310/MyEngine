#pragma once

#include "ecs/Scene.h"
#include <string>
#include <memory>

namespace MyEngine
{
	class Shader;

	namespace Serialization
	{
		bool SaveScene(const ::Scene& scene, const std::string& path);
		// defaultShader is assigned to each loaded entity's MeshRendererComponent
		// since the shader itself is not persisted in the scene file.
		bool LoadScene(::Scene& scene, const std::string& path, const std::shared_ptr<MyEngine::Shader>& defaultShader = nullptr);
	}
}
