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
	class Skeleton;
	class AnimationClip;
	class Material;

	// Result of loading a rigged/animated model: meshes plus the shared
	// skeleton and animation clips extracted from the same source file.
	// `skeleton` is null and `clips` is empty for models with no bone data.
	struct SkinnedModelData
	{
		std::vector<std::shared_ptr<Mesh>> meshes;
		std::shared_ptr<Skeleton> skeleton;
		std::shared_ptr<std::vector<AnimationClip>> clips;
	};

	class AssetManager
	{
	public:
		// Load model and return its meshes (cached)
		static std::vector<std::shared_ptr<Mesh>> LoadModel(const std::string& path);

		// Load a rigged/animated model, returning meshes + skeleton + animation
		// clips together (cached). Use this instead of LoadModel when the file
		// may contain bone weights/animations you want to preserve.
		static SkinnedModelData LoadSkinnedModel(const std::string& path);

		// Load animation clips from a model file (glTF/GLB/FBX, etc.). This is a
		// lightweight convenience wrapper over LoadSkinnedModel for attaching
		// external animation clips to an existing animated entity.
		static std::shared_ptr<std::vector<AnimationClip>> LoadAnimationClips(const std::string& path);

		// Returns true if a clip has enough track overlap with the given skeleton
		// to be considered compatible for import/runtime playback.
		static bool IsAnimationClipCompatible(const AnimationClip& clip, const std::shared_ptr<Skeleton>& skeleton, float minimumTrackMatchRatio = 0.35f);

		// Counts how many tracks in the clip resolve to bones on the skeleton.
		static int CountMatchingAnimationTracks(const AnimationClip& clip, const std::shared_ptr<Skeleton>& skeleton);

		// Load texture (cached)
		static std::shared_ptr<Texture> LoadTexture(const std::string& path, bool generateMipmaps = true);

		// Load/create a shader from vertex+fragment paths (cached by combined path)
		static std::shared_ptr<Shader> LoadShader(const std::string& vertexPath, const std::string& fragmentPath);

		// Load a material asset (cached by path)
		static std::shared_ptr<Material> LoadMaterial(const std::string& path);

		// Import materials embedded in a model file.
		// For each mesh in the model a Material asset is created (or reused if it
		// already exists) under `outputDir/<modelStem>_<index>.material.json`.
		// The returned vector is ordered to match the model's mesh list.
		// Pass an empty outputDir to default to "assets/materials".
		static std::vector<std::shared_ptr<Material>> ImportModelMaterials(
			const std::string& modelPath,
			const std::string& outputDir = "");

		// Load an audio clip (WAV) from disk (cached by path)
		static std::shared_ptr<AudioClip> LoadAudioClip(const std::string& path);

		// Attach a mesh to an entity and automatically add MeshComponent, MeshRendererComponent (if shader provided),
		// and BoundingSphereComponent derived from the mesh bounds. assetPath may be empty.
		static void AttachMeshToEntity(const std::shared_ptr<::Entity>& entity,
										const std::shared_ptr<Mesh>& mesh,
										const std::string& assetPath = "",
										const std::shared_ptr<Shader>& shader = nullptr);

		// Attach the first mesh of a skinned model to an entity along with
		// MeshComponent/MeshRendererComponent/BoundingSphereComponent (like
		// AttachMeshToEntity), plus SkeletonComponent and AnimationComponent
		// wired to the model's skeleton/clips. If `data.skeleton` is null this
		// behaves like a plain AttachMeshToEntity (no animation components added).
		// `assetPath` is stored on MeshComponent so scenes can be serialized/
		// reloaded (see SceneSerializer) - pass the same path used to load
		// `data` via LoadSkinnedModel.
		static void AttachSkinnedModelToEntity(const std::shared_ptr<::Entity>& entity,
										const SkinnedModelData& data,
										const std::shared_ptr<Shader>& shader = nullptr,
										const std::string& assetPath = "");

	private:
		static std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> s_ModelCache;
		static std::unordered_map<std::string, SkinnedModelData> s_SkinnedModelCache;
		static std::unordered_map<std::string, std::shared_ptr<Texture>> s_TextureCache;
		static std::unordered_map<std::string, std::shared_ptr<Shader>> s_ShaderCache;
		static std::unordered_map<std::string, std::shared_ptr<Material>> s_MaterialCache;
		static std::unordered_map<std::string, std::shared_ptr<AudioClip>> s_AudioClipCache;
	};
}
