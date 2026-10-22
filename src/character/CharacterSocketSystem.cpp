#include "character/CharacterSocketSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "components/AnimationComponent.h"
#include "components/CharacterCustomizationComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/SkeletonComponent.h"
#include "components/SocketAttachmentComponent.h"
#include "components/TransformComponent.h"
#include "core/AssetManager.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"
#include "ecs/TransformHierarchy.h"
#include "rendering/Mesh.h"
#include "rendering/MeshPrimitives.h"
#include "rendering/Skeleton.h"

namespace MyEngine
{
	namespace
	{
		std::shared_ptr<Mesh> DefaultLoadMesh(const std::string& meshPath)
		{
			if (meshPath.empty())
				return nullptr;
			if (meshPath == "primitive_cube")
				return MeshPrimitives::CreateCube();
			if (meshPath == "primitive_sphere")
				return MeshPrimitives::CreateSphere();
			if (meshPath == "primitive_plane")
				return MeshPrimitives::CreatePlane(0.5f);

			auto meshes = AssetManager::LoadModel(meshPath);
			return meshes.empty() ? nullptr : meshes.front();
		}

		void WriteWorldTransform(TransformComponent& transform, const glm::mat4& world)
		{
			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			if (!glm::decompose(world, scale, rotation, translation, skew, perspective))
				return;

			transform.parentID = 0;
			transform.position = translation;
			transform.rotation = glm::eulerAngles(rotation);
			transform.scale = scale;
		}

		std::shared_ptr<Entity> FindAttachment(Scene& scene, uint32_t ownerID, const std::string& socketName)
		{
			for (const auto& entity : scene.GetEntities())
			{
				if (!entity || !entity->HasComponent<SocketAttachmentComponent>())
					continue;
				const auto& attachment = entity->GetComponent<SocketAttachmentComponent>();
				if (attachment.ownerEntityID == ownerID && attachment.socketName == socketName)
					return entity;
			}
			return nullptr;
		}
	}

	CharacterSocketSystem::CharacterSocketSystem()
		: m_MeshLoader(&DefaultLoadMesh)
	{
	}

	std::shared_ptr<Mesh> CharacterSocketSystem::LoadMesh(const std::string& meshPath)
	{
		return m_MeshLoader ? m_MeshLoader(meshPath) : nullptr;
	}

	bool CharacterSocketSystem::ComputeSocketWorldMatrix(
		const Scene& scene,
		const Entity& owner,
		const std::string& boneName,
		const glm::mat4& socketLocal,
		glm::mat4& outWorld)
	{
		if (!owner.HasComponent<SkeletonComponent>())
			return false;
		const auto& skeleton = owner.GetComponent<SkeletonComponent>().skeleton;
		if (!skeleton)
			return false;

		const int boneIndex = skeleton->GetBoneIndex(boneName);
		if (boneIndex < 0)
			return false;

		const auto& bone = skeleton->GetBones()[boneIndex];
		glm::mat4 boneModel = glm::inverse(bone.offsetMatrix);
		if (owner.HasComponent<AnimationComponent>())
		{
			const auto& anim = owner.GetComponent<AnimationComponent>();
			if (static_cast<int>(anim.boneMatrices.size()) > boneIndex)
				boneModel = anim.boneMatrices[boneIndex] * boneModel;
		}

		const glm::mat4 ownerWorld = TransformHierarchy::GetWorldMatrix(scene, owner);
		outWorld = ownerWorld * boneModel * socketLocal;
		return true;
	}

	void CharacterSocketSystem::Update(Scene& scene)
	{
		// Snapshot owners first: creating attachment entities mutates the list.
		std::vector<std::shared_ptr<Entity>> owners;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity
				&& entity->HasComponent<CharacterCustomizationComponent>()
				&& entity->HasComponent<SkeletonComponent>()
				&& !entity->HasComponent<SocketAttachmentComponent>())
			{
				owners.push_back(entity);
			}
		}

		for (const auto& owner : owners)
		{
			auto& customization = owner->GetComponent<CharacterCustomizationComponent>();
			for (const auto& socket : customization.sockets)
			{
				if (socket.name.empty())
					continue;

				std::shared_ptr<Entity> attachment = FindAttachment(scene, owner->GetID(), socket.name);
				if (!attachment)
				{
					attachment = scene.CreateEntity(owner->GetName() + ":" + socket.name);
					if (!attachment)
						continue;
					attachment->SetTag("SocketAttachment");
					attachment->AddComponent<TransformComponent>();
					auto& marker = attachment->AddComponent<SocketAttachmentComponent>();
					marker.ownerEntityID = owner->GetID();
					marker.socketName = socket.name;
				}

				auto& marker = attachment->GetComponent<SocketAttachmentComponent>();
				if (marker.loadedMeshPath != socket.meshPath)
				{
					marker.loadedMeshPath = socket.meshPath;
					if (auto mesh = LoadMesh(socket.meshPath))
					{
						AssetManager::AttachMeshToEntity(attachment, mesh, socket.meshPath, m_DefaultShader);
					}
					else
					{
						attachment->RemoveComponent<MeshComponent>();
						attachment->RemoveComponent<MeshRendererComponent>();
					}
				}

				if (attachment->HasComponent<MeshRendererComponent>())
					attachment->GetComponent<MeshRendererComponent>().visible = socket.visible && customization.enabled;

				glm::mat4 world;
				if (ComputeSocketWorldMatrix(scene, *owner, socket.boneName, socket.BuildLocalMatrix(), world))
				{
					if (!attachment->HasComponent<TransformComponent>())
						attachment->AddComponent<TransformComponent>();
					WriteWorldTransform(attachment->GetComponent<TransformComponent>(), world);
				}
				else if (attachment->HasComponent<MeshRendererComponent>())
				{
					attachment->GetComponent<MeshRendererComponent>().visible = false;
				}
			}
		}

		// Remove orphaned attachments (owner gone, socket removed/renamed).
		std::vector<uint32_t> toDestroy;
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity || !entity->HasComponent<SocketAttachmentComponent>())
				continue;
			const auto& marker = entity->GetComponent<SocketAttachmentComponent>();
			Entity* owner = scene.GetEntityByID(marker.ownerEntityID);
			bool alive = owner
				&& owner->HasComponent<CharacterCustomizationComponent>()
				&& owner->HasComponent<SkeletonComponent>();
			if (alive)
			{
				const auto& sockets = owner->GetComponent<CharacterCustomizationComponent>().sockets;
				alive = std::any_of(sockets.begin(), sockets.end(), [&](const BoneSocket& s) { return s.name == marker.socketName; });
			}
			if (!alive)
				toDestroy.push_back(entity->GetID());
		}
		for (uint32_t id : toDestroy)
			scene.DestroyEntity(id);
	}

	void CharacterSocketSystem::DestroyAllAttachments(Scene& scene)
	{
		std::vector<uint32_t> toDestroy;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity && entity->HasComponent<SocketAttachmentComponent>())
				toDestroy.push_back(entity->GetID());
		}
		for (uint32_t id : toDestroy)
			scene.DestroyEntity(id);
	}
}
