#pragma once

#include <functional>
#include <memory>
#include <string>

#include <glm/glm.hpp>

class Scene;
class Entity;

namespace MyEngine
{
	class Mesh;
	class Shader;
	class Skeleton;

	// Keeps bone-socket attachment entities in sync with each skinned
	// entity's CharacterCustomizationComponent::sockets.
	//
	// Per frame (after AnimationSystem::Update so bone palettes are fresh):
	//   * creates a SocketAttachmentComponent entity for every socket that lacks one
	//   * (re)loads the socket mesh when the path changes
	//   * writes the attachment's world TRS from ownerWorld * boneModel * socketLocal
	//   * destroys attachments whose owner/socket no longer exists
	class CharacterSocketSystem
	{
	public:
		using MeshLoader = std::function<std::shared_ptr<Mesh>(const std::string& meshPath)>;

		CharacterSocketSystem();

		// Shader applied to newly created attachment entities (may be null).
		void SetDefaultShader(const std::shared_ptr<Shader>& shader) { m_DefaultShader = shader; }

		// Overrides how meshes are resolved from socket paths (tests inject a
		// GL-free loader). Default resolves "primitive_*" names and model files
		// through AssetManager.
		void SetMeshLoader(MeshLoader loader) { m_MeshLoader = std::move(loader); }

		void Update(Scene& scene);

		// Removes every attachment entity in the scene (e.g. before saving or
		// when switching scenes).
		static void DestroyAllAttachments(Scene& scene);

		// World matrix for a socket on `owner`, or identity when the bone is
		// missing. Exposed for the editor overlay/gizmo.
		static bool ComputeSocketWorldMatrix(
			const Scene& scene,
			const Entity& owner,
			const std::string& boneName,
			const glm::mat4& socketLocal,
			glm::mat4& outWorld);

	private:
		std::shared_ptr<Mesh> LoadMesh(const std::string& meshPath);

		std::shared_ptr<Shader> m_DefaultShader;
		MeshLoader m_MeshLoader;
	};
}
