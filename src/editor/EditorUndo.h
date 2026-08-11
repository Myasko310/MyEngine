#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "components/TransformComponent.h"

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
