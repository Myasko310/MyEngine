#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "ecs/Entity.h"

namespace MyEngine
{
	class Mesh;
	class Shader;
	class Texture;
	class AudioClip;

	class AssetManager
	{
	public:
		// Load model and return its meshes (cached)
		static std::vector<std::shared_ptr<Mesh>> LoadModel(const std::string& path);

		// Load texture (cached)
		static std::shared_ptr<Texture> LoadTexture(const std::string& path, bool generateMipmaps = true);

		// Load/create a shader from vertex+fragment paths (cached by combined path)
		static std::shared_ptr<Shader> LoadShader(const std::string& vertexPath, const std::string& fragmentPath);

		// Load an audio clip (WAV) from disk (cached by path)
		static std::shared_ptr<AudioClip> LoadAudioClip(const std::string& path);

		// Attach a mesh to an entity and automatically add MeshComponent, MeshRendererComponent (if shader provided),
		// and BoundingSphereComponent derived from the mesh bounds. assetPath may be empty.
		static void AttachMeshToEntity(const std::shared_ptr<::Entity>& entity,
										const std::shared_ptr<Mesh>& mesh,
										const std::string& assetPath = "",
										const std::shared_ptr<Shader>& shader = nullptr);

	private:
		static std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> s_ModelCache;
		static std::unordered_map<std::string, std::shared_ptr<Texture>> s_TextureCache;
		static std::unordered_map<std::string, std::shared_ptr<Shader>> s_ShaderCache;
		static std::unordered_map<std::string, std::shared_ptr<AudioClip>> s_AudioClipCache;
	};
}
