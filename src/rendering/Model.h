#pragma once

#include <string>
#include <vector>
#include <memory>

#include "rendering/Mesh.h"

namespace MyEngine
{
	class Model
	{
	public:
		Model() = default;

		bool LoadFromFile(const std::string& path);

		const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }

	private:
		std::vector<std::shared_ptr<Mesh>> m_Meshes;
	};
}
