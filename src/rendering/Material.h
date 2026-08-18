#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace MyEngine
{
	class Shader;
	class Texture;

	enum class BlendMode
	{
		Opaque     = 0,  // GL_NONE, no blending
		AlphaBlend = 1,  // standard alpha compositing
		Additive   = 2   // additive / particle / glow
	};

	enum class CullMode
	{
		Back  = 0,  // default back-face culling
		Front = 1,  // front-face culling (e.g. inner surfaces)
		Off   = 2   // double-sided
	};

	class Material
	{
	public:
		Material() = default;
		explicit Material(std::string path)
			: m_Path(std::move(path))
		{
		}

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		std::shared_ptr<Shader> shader = nullptr;
		std::string shaderVertexPath;
		std::string shaderFragmentPath;

		glm::vec3 albedo = glm::vec3(1.0f);
		float shininess = 32.0f;
		std::shared_ptr<Texture> texture = nullptr;
		bool useTexture = false;

		bool usePBR = false;
		float metallic = 0.0f;
		float roughness = 0.5f;
		float aoStrength = 1.0f;
		glm::vec3 emissive = glm::vec3(0.0f);

		std::shared_ptr<Texture> albedoMap = nullptr;
		std::shared_ptr<Texture> normalMap = nullptr;
		std::shared_ptr<Texture> metallicRoughnessMap = nullptr;
		std::shared_ptr<Texture> aoMap = nullptr;
		std::shared_ptr<Texture> emissiveMap = nullptr;

		// --- Render flags ---
		BlendMode blendMode   = BlendMode::Opaque;
		CullMode  cullMode    = CullMode::Back;
		bool      depthWrite  = true;
		bool      depthTest   = true;
		int       renderQueue = 2000; // lower = earlier draw; use >2500 for transparent

	private:
		std::string m_Path;
	};
}
