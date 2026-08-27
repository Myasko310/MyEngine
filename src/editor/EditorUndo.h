#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "components/TransformComponent.h"
#include "core/AssetManager.h"
#include "rendering/Material.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"
#include "serialization/SceneSerializer.h"
#include "systems/ScriptSystem.h"

// Minimal editor undo/redo stack.
// Currently supports transform edits (gizmo/inspector drags); the command
// interface allows more command types to be added later.
namespace EditorUndo
{
	class Command
	{
	public:
		virtual ~Command() = default;
		virtual void Undo(Scene& scene) = 0;
		virtual void Redo(Scene& scene) = 0;
	};

	class TransformEditCommand : public Command
	{
	public:
		TransformEditCommand(uint32_t entityID,
			const TransformComponent& before,
			const TransformComponent& after)
			: m_EntityID(entityID), m_Before(before), m_After(after)
		{
		}

		void Undo(Scene& scene) override { Apply(scene, m_Before); }
		void Redo(Scene& scene) override { Apply(scene, m_After); }

	private:
		void Apply(Scene& scene, const TransformComponent& state)
		{
			auto entity = TransformHierarchy::FindEntityByID(scene, m_EntityID);
			if (entity && entity->HasComponent<TransformComponent>())
				entity->GetComponent<TransformComponent>() = state;
		}

		uint32_t m_EntityID;
		TransformComponent m_Before;
		TransformComponent m_After;
	};

	class SceneStateCommand : public Command
	{
	public:
		SceneStateCommand(
			const std::string& beforeState,
			const std::string& afterState,
			const std::shared_ptr<MyEngine::Shader>& shader,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* globalScripts,
			Entity** selectedEntity,
			std::shared_ptr<Entity>* playerEntity)
			: m_BeforeState(beforeState)
			, m_AfterState(afterState)
			, m_Shader(shader)
			, m_GlobalScripts(globalScripts)
			, m_SelectedEntity(selectedEntity)
			, m_PlayerEntity(playerEntity)
		{
		}

		void Undo(Scene& scene) override { Apply(scene, m_BeforeState); }
		void Redo(Scene& scene) override { Apply(scene, m_AfterState); }

	private:
		void Apply(Scene& scene, const std::string& state)
		{
			if (!m_GlobalScripts)
				return;

			MyEngine::Serialization::LoadSceneFromString(scene, state, m_Shader, m_GlobalScripts);

			if (m_SelectedEntity)
				*m_SelectedEntity = nullptr;

			if (m_PlayerEntity)
			{
				m_PlayerEntity->reset();
				for (auto& e : scene.GetEntities())
				{
					if (e && e->GetName() == "Player")
					{
						*m_PlayerEntity = e;
						break;
					}
				}
			}
		}

		std::string m_BeforeState;
		std::string m_AfterState;
		std::shared_ptr<MyEngine::Shader> m_Shader;
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* m_GlobalScripts = nullptr;
		Entity** m_SelectedEntity = nullptr;
		std::shared_ptr<Entity>* m_PlayerEntity = nullptr;
	};

	class GlobalScriptsCommand : public Command
	{
	public:
		GlobalScriptsCommand(
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& before,
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& after,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* target)
			: m_Before(before), m_After(after), m_Target(target)
		{
		}

		void Undo(Scene&) override { if (m_Target) *m_Target = m_Before; }
		void Redo(Scene&) override { if (m_Target) *m_Target = m_After; }

	private:
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> m_Before;
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> m_After;
		std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* m_Target = nullptr;
	};

	struct MaterialSnapshot
	{
		std::string path;
		std::string shaderVertexPath;
		std::string shaderFragmentPath;
		glm::vec3 albedo = glm::vec3(1.0f);
		float shininess = 32.0f;
		bool useTexture = false;
		bool usePBR = false;
		float metallic = 0.0f;
		float roughness = 0.5f;
		float aoStrength = 1.0f;
		glm::vec3 emissive = glm::vec3(0.0f);
		MyEngine::BlendMode blendMode = MyEngine::BlendMode::Opaque;
		MyEngine::CullMode cullMode = MyEngine::CullMode::Back;
		bool depthWrite = true;
		bool depthTest = true;
		int renderQueue = 2000;
		std::string texturePath;
		std::string albedoMapPath;
		std::string normalMapPath;
		std::string metallicRoughnessMapPath;
		std::string aoMapPath;
		std::string emissiveMapPath;
	};

	class MaterialEditCommand : public Command
	{
	public:
		MaterialEditCommand(
			const std::shared_ptr<MyEngine::Material>& target,
			const MaterialSnapshot& before,
			const MaterialSnapshot& after)
			: m_Target(target), m_Before(before), m_After(after)
		{
		}

		void Undo(Scene&) override { Apply(m_Before); }
		void Redo(Scene&) override { Apply(m_After); }

		static MaterialSnapshot Capture(const MyEngine::Material& material)
		{
			MaterialSnapshot snapshot;
			snapshot.path = material.GetPath();
			snapshot.shaderVertexPath = material.shaderVertexPath;
			snapshot.shaderFragmentPath = material.shaderFragmentPath;
			snapshot.albedo = material.albedo;
			snapshot.shininess = material.shininess;
			snapshot.useTexture = material.useTexture;
			snapshot.usePBR = material.usePBR;
			snapshot.metallic = material.metallic;
			snapshot.roughness = material.roughness;
			snapshot.aoStrength = material.aoStrength;
			snapshot.emissive = material.emissive;
			snapshot.blendMode = material.blendMode;
			snapshot.cullMode = material.cullMode;
			snapshot.depthWrite = material.depthWrite;
			snapshot.depthTest = material.depthTest;
			snapshot.renderQueue = material.renderQueue;
			snapshot.texturePath = material.texture ? material.texture->GetPath() : std::string{};
			snapshot.albedoMapPath = material.albedoMap ? material.albedoMap->GetPath() : std::string{};
			snapshot.normalMapPath = material.normalMap ? material.normalMap->GetPath() : std::string{};
			snapshot.metallicRoughnessMapPath = material.metallicRoughnessMap ? material.metallicRoughnessMap->GetPath() : std::string{};
			snapshot.aoMapPath = material.aoMap ? material.aoMap->GetPath() : std::string{};
			snapshot.emissiveMapPath = material.emissiveMap ? material.emissiveMap->GetPath() : std::string{};
			return snapshot;
		}

	private:
		static std::shared_ptr<MyEngine::Texture> LoadTextureIfPresent(const std::string& path)
		{
			if (path.empty())
				return nullptr;
			try
			{
				return MyEngine::AssetManager::LoadTexture(path);
			}
			catch (...)
			{
				return nullptr;
			}
		}

		void Apply(const MaterialSnapshot& snapshot)
		{
			if (!m_Target)
				return;

			m_Target->SetPath(snapshot.path);
			m_Target->shaderVertexPath = snapshot.shaderVertexPath;
			m_Target->shaderFragmentPath = snapshot.shaderFragmentPath;
			if (!snapshot.shaderVertexPath.empty() && !snapshot.shaderFragmentPath.empty())
			{
				try
				{
					m_Target->shader = MyEngine::AssetManager::LoadShader(snapshot.shaderVertexPath, snapshot.shaderFragmentPath);
				}
				catch (...)
				{
					m_Target->shader = nullptr;
				}
			}
			else
			{
				m_Target->shader = nullptr;
			}

			m_Target->albedo = snapshot.albedo;
			m_Target->shininess = snapshot.shininess;
			m_Target->useTexture = snapshot.useTexture;
			m_Target->usePBR = snapshot.usePBR;
			m_Target->metallic = snapshot.metallic;
			m_Target->roughness = snapshot.roughness;
			m_Target->aoStrength = snapshot.aoStrength;
			m_Target->emissive = snapshot.emissive;
			m_Target->blendMode = snapshot.blendMode;
			m_Target->cullMode = snapshot.cullMode;
			m_Target->depthWrite = snapshot.depthWrite;
			m_Target->depthTest = snapshot.depthTest;
			m_Target->renderQueue = snapshot.renderQueue;
			m_Target->texture = LoadTextureIfPresent(snapshot.texturePath);
			m_Target->albedoMap = LoadTextureIfPresent(snapshot.albedoMapPath);
			m_Target->normalMap = LoadTextureIfPresent(snapshot.normalMapPath);
			m_Target->metallicRoughnessMap = LoadTextureIfPresent(snapshot.metallicRoughnessMapPath);
			m_Target->aoMap = LoadTextureIfPresent(snapshot.aoMapPath);
			m_Target->emissiveMap = LoadTextureIfPresent(snapshot.emissiveMapPath);
		}

		std::shared_ptr<MyEngine::Material> m_Target;
		MaterialSnapshot m_Before;
		MaterialSnapshot m_After;
	};

	class UndoStack
	{
	public:
		void Push(std::unique_ptr<Command> command)
		{
			m_Undo.push_back(std::move(command));
			m_Redo.clear();
			if (m_Undo.size() > kMaxCommands)
				m_Undo.erase(m_Undo.begin());
		}

		bool CanUndo() const { return !m_Undo.empty(); }
		bool CanRedo() const { return !m_Redo.empty(); }

		void Undo(Scene& scene)
		{
			if (m_Undo.empty())
				return;
			auto cmd = std::move(m_Undo.back());
			m_Undo.pop_back();
			cmd->Undo(scene);
			m_Redo.push_back(std::move(cmd));
		}

		void Redo(Scene& scene)
		{
			if (m_Redo.empty())
				return;
			auto cmd = std::move(m_Redo.back());
			m_Redo.pop_back();
			cmd->Redo(scene);
			m_Undo.push_back(std::move(cmd));
		}

		void Clear()
		{
			m_Undo.clear();
			m_Redo.clear();
		}

	private:
		static constexpr size_t kMaxCommands = 128;
		std::vector<std::unique_ptr<Command>> m_Undo;
		std::vector<std::unique_ptr<Command>> m_Redo;
	};
}
