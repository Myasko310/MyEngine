#include "editor/PlayModeSession.h"

#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "core/FileDialog.h"
#include "serialization/SceneSerializer.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace MyEngine::Editor
{
	void PlayModeSession::CaptureSnapshot(const ::Scene& scene, const std::vector<ScriptSystem::GlobalScriptConfig>& globalScripts)
	{
		m_SnapshotJson = MyEngine::Serialization::SaveSceneToString(scene, globalScripts);
		m_HasSnapshot = !m_SnapshotJson.empty();
		m_StopPromptPending = false;
	}

	void PlayModeSession::RestoreSnapshot(PlayModeRestoreContext& context)
	{
		if (!m_HasSnapshot || !context.scene || !context.globalScripts)
			return;

		bool hadCamera = false;
		TransformComponent savedCameraTransform;
		CameraComponent savedCamera;
		for (auto& e : context.scene->GetEntities())
		{
			if (e && e->HasComponent<CameraComponent>() && e->GetComponent<CameraComponent>().isPrimary)
			{
				if (e->HasComponent<TransformComponent>())
					savedCameraTransform = e->GetComponent<TransformComponent>();
				savedCamera = e->GetComponent<CameraComponent>();
				hadCamera = true;
				break;
			}
		}

		if (context.selectedEntity)
			*context.selectedEntity = nullptr;
		if (context.undoStack)
			context.undoStack->Clear();

		std::vector<uint32_t> ids;
		for (const auto& e : context.scene->GetEntities())
			if (e) ids.push_back(e->GetID());
		for (uint32_t id : ids)
			context.scene->DestroyEntity(id);

		MyEngine::Serialization::LoadSceneFromString(*context.scene, m_SnapshotJson, context.defaultShader, context.globalScripts);
		m_HasSnapshot = false;
		m_SnapshotJson.clear();
		m_StopPromptPending = false;

		if (context.playerEntity)
		{
			context.playerEntity->reset();
			for (auto& e : context.scene->GetEntities())
			{
				if (e && e->GetName() == "Player")
				{
					*context.playerEntity = e;
					break;
				}
			}
		}

		for (auto& e : context.scene->GetEntities())
		{
			if (e && e->HasComponent<CameraComponent>())
			{
				auto& freshCamera = e->GetComponent<CameraComponent>();
				if (context.playerEntity && *context.playerEntity)
					freshCamera.followTargetID = (*context.playerEntity)->GetID();

				if (hadCamera)
				{
					if (e->HasComponent<TransformComponent>())
						e->GetComponent<TransformComponent>() = savedCameraTransform;

					freshCamera.yaw = savedCamera.yaw;
					freshCamera.pitch = savedCamera.pitch;
					freshCamera.thirdPerson = savedCamera.thirdPerson;
					freshCamera.followDistance = savedCamera.followDistance;
					freshCamera.followHeight = savedCamera.followHeight;
					freshCamera.smoothedMouseDelta = savedCamera.smoothedMouseDelta;
				}
				break;
			}
		}
	}

	bool PlayModeSession::HasSnapshot() const
	{
		return m_HasSnapshot;
	}

	const std::string& PlayModeSession::GetSnapshotJson() const
	{
		return m_SnapshotJson;
	}

	void PlayModeSession::ClearSnapshot()
	{
		m_HasSnapshot = false;
		m_SnapshotJson.clear();
		m_StopPromptPending = false;
	}

	void PlayModeSession::RequestStopPrompt()
	{
		m_StopPromptPending = true;
	}

	bool PlayModeSession::IsStopPromptPending() const
	{
		return m_StopPromptPending;
	}

#ifdef USE_IMGUI
	void PlayModeSession::DrawStopPlayPrompt(StopPlayPromptContext& context)
	{
		if (m_StopPromptPending)
		{
			ImGui::OpenPopup("Stop Play Mode");
			m_StopPromptPending = false;
		}

		bool keepStopPlayPopupOpen = true;
		if (!ImGui::BeginPopupModal("Stop Play Mode", &keepStopPlayPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
			return;

		ImGui::TextWrapped("Play mode changes were detected. Save runtime changes as a separate scene copy before restoring edit snapshot?");
		ImGui::Separator();

		if (ImGui::Button("Save Runtime Scene Copy...", ImVec2(220, 0)))
		{
			std::string saveCopyPath = MyEngine::FileDialog::SaveSceneFile();
			if (!saveCopyPath.empty() && context.scene && context.globalScripts)
			{
				MyEngine::Serialization::SaveScene(*context.scene, saveCopyPath, *context.globalScripts);
				if (context.recentScenes && context.addRecentScene)
					context.addRecentScene(*context.recentScenes, saveCopyPath);
			}
			RestoreSnapshot(context.restoreContext);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::Button("Discard Runtime Changes", ImVec2(220, 0)))
		{
			RestoreSnapshot(context.restoreContext);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::Button("Cancel", ImVec2(220, 0)))
		{
			if (context.isPlaying)
				*context.isPlaying = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
#endif
}
