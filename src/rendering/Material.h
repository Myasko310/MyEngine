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

		// Material instance inheritance
		bool useBaseMaterial = false;
		std::string baseMaterialPath;
		std::shared_ptr<Material> baseMaterial = nullptr;
		bool overrideShader = true;
		bool overrideSurface = true;
		bool overrideTextures = true;
		bool overridePBR = true;
		bool overrideRenderFlags = true;

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

		std::shared_ptr<Shader> GetResolvedShader() const;
		glm::vec3 GetResolvedAlbedo() const;
		float GetResolvedShininess() const;
		std::shared_ptr<Texture> GetResolvedTexture() const;
		bool GetResolvedUseTexture() const;
		bool GetResolvedUsePBR() const;
		float GetResolvedMetallic() const;
		float GetResolvedRoughness() const;
		float GetResolvedAOStrength() const;
		glm::vec3 GetResolvedEmissive() const;
		std::shared_ptr<Texture> GetResolvedAlbedoMap() const;
		std::shared_ptr<Texture> GetResolvedNormalMap() const;
		std::shared_ptr<Texture> GetResolvedMetallicRoughnessMap() const;
		std::shared_ptr<Texture> GetResolvedAOMap() const;
		std::shared_ptr<Texture> GetResolvedEmissiveMap() const;
		BlendMode GetResolvedBlendMode() const;
		CullMode GetResolvedCullMode() const;
		bool GetResolvedDepthWrite() const;
		bool GetResolvedDepthTest() const;
		int GetResolvedRenderQueue() const;

	private:
		const Material* GetBaseMaterialResolved() const;
		std::string m_Path;
	};
}
