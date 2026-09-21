#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

#include "network/NetReplicationSystem.h"
#include "network/NetTransport.h"

class Scene;
class MeshRendererSystem;

namespace MyEngine
{
	class PhysicsSystem;

	namespace Plugins
	{
		class PluginManager;
	}

	namespace Editor::Panels
	{
		struct FramePacingDiagnostics
		{
			static constexpr int kSampleWindow = 240;
			std::array<float, kSampleWindow> frameMsSamples{};
			int nextSampleIndex = 0;
			int sampleCount = 0;

			float averageFrameMs = 0.0f;
			float minFrameMs = 0.0f;
			float maxFrameMs = 0.0f;
			float stdDevFrameMs = 0.0f;
			float averageJitterMs = 0.0f;
			int spikeCountOver16Ms = 0;
			int spikeCountOver33Ms = 0;

			float cameraStepPerSec = 0.0f;
			float cameraStepJitterPerSec = 0.0f;
			float worldStepPerSec = 0.0f;
			float worldStepJitterPerSec = 0.0f;
			int cameraLargeStepCount = 0;
			int worldLargeStepCount = 0;

			void AddFrameSample(float frameMs)
			{
				frameMsSamples[nextSampleIndex] = frameMs;
				nextSampleIndex = (nextSampleIndex + 1) % kSampleWindow;
				if (sampleCount < kSampleWindow)
					++sampleCount;

				RecalculateRollingStats();
			}

			void AddCameraPositionSample(const glm::vec3& worldPosition, float deltaTime)
			{
				AddSpatialSample(worldPosition, deltaTime, previousCameraPosition, hasPreviousCameraPosition, previousCameraStepPerSec, hasPreviousCameraStepPerSec,
					cameraStepPerSec, cameraStepJitterPerSec, cameraLargeStepCount);
			}

			void AddWorldPositionSample(const glm::vec3& worldPosition, float deltaTime)
			{
				AddSpatialSample(worldPosition, deltaTime, previousWorldPosition, hasPreviousWorldPosition, previousWorldStepPerSec, hasPreviousWorldStepPerSec,
					worldStepPerSec, worldStepJitterPerSec, worldLargeStepCount);
			}

		private:
			glm::vec3 previousCameraPosition = glm::vec3(0.0f);
			glm::vec3 previousWorldPosition = glm::vec3(0.0f);
			float previousCameraStepPerSec = 0.0f;
			float previousWorldStepPerSec = 0.0f;
			bool hasPreviousCameraPosition = false;
			bool hasPreviousWorldPosition = false;
			bool hasPreviousCameraStepPerSec = false;
			bool hasPreviousWorldStepPerSec = false;

			static void AddSpatialSample(
				const glm::vec3& worldPosition,
				float deltaTime,
				glm::vec3& previousPosition,
				bool& hasPreviousPosition,
				float& previousStepPerSec,
				bool& hasPreviousStepPerSec,
				float& outCurrentStepPerSec,
				float& outCurrentJitterPerSec,
				int& outLargeStepCount)
			{
				if (!hasPreviousPosition || deltaTime <= 0.00001f)
				{
					previousPosition = worldPosition;
					hasPreviousPosition = true;
					outCurrentStepPerSec = 0.0f;
					outCurrentJitterPerSec = 0.0f;
					return;
				}

				const float distance = glm::length(worldPosition - previousPosition);
				const float speedEquivalent = distance / deltaTime;
				outCurrentStepPerSec = speedEquivalent;

				if (hasPreviousStepPerSec)
					outCurrentJitterPerSec = std::abs(speedEquivalent - previousStepPerSec);
				else
					outCurrentJitterPerSec = 0.0f;

				previousStepPerSec = speedEquivalent;
				hasPreviousStepPerSec = true;
				previousPosition = worldPosition;

				if (distance > 0.25f)
					++outLargeStepCount;
			}

			float GetChronologicalSample(int orderedIndex) const;
			void RecalculateRollingStats();
		};

		struct PerformancePanelContext
		{
			bool* isOpen = nullptr;
			::Scene* scene = nullptr;
			::MeshRendererSystem* renderSystem = nullptr;
			PhysicsSystem* physicsSystem = nullptr;
			Net::INetTransport* networkTransport = nullptr;
			Plugins::PluginManager* pluginManager = nullptr;

			std::function<void()> loadPlugins;
			std::function<void()> reloadPlugins;
			std::function<void()> unloadPlugins;
			std::function<bool()> exportPerformanceCsv;

			float deltaTime = 0.0f;
			float gpuFrameMs = 0.0f;
			float cpuPhysicsMs = 0.0f;
			float cpuAnimationMs = 0.0f;
			float cpuRenderMs = 0.0f;
			std::uint32_t replayParticleSeed = 0u;
			FramePacingDiagnostics* framePacingDiagnostics = nullptr;

			std::size_t pendingTerrainPatchJobCount = 0;
			std::size_t terrainPatchQueuePeak = 0;
			float terrainPatchPrepMs = 0.0f;
			float terrainPatchUploadMs = 0.0f;
			std::size_t terrainPatchBackpressureDeferrals = 0;
			int* terrainPatchMaxQueuedJobs = nullptr;
			float navMeshRebuildMs = 0.0f;
			bool navMeshLastRebuildWasRegion = false;

			float memoryWorkingSetMB = 0.0f;
			float memoryPrivateMB = 0.0f;

			const float* frameMsHistory = nullptr;
			const float* physicsMsHistory = nullptr;
			const float* animationMsHistory = nullptr;
			const float* renderMsHistory = nullptr;
			const float* visibleCountHistory = nullptr;
			const float* occlusionRejectHistory = nullptr;
			const float* frustumRejectHistory = nullptr;
			std::size_t perfHistoryCount = 0;
			int perfHistoryIndex = 0;

			bool networkSessionBootstrapPending = false;
			bool usingSocketTransport = false;
			Net::ClientID localClientID = 0u;
			Net::NetTick networkTick = 0u;
			bool hasAuthoritativeServerScene = false;
			Net::NetTick* transportInputDelayTicks = nullptr;
			Net::NetTick* transportSnapshotDelayTicks = nullptr;
			bool* networkClientSmoothingEnabled = nullptr;
			float* networkClientPositionSmoothingRate = nullptr;
			float* networkClientRotationSmoothingRate = nullptr;
			float* networkClientPositionSnapDistance = nullptr;
			float* networkClientRotationSnapDistance = nullptr;
			Net::ReplicationInterestSettings* networkInterestSettings = nullptr;

			int viewportWidth = 0;
			int viewportHeight = 0;
			bool* wireframe = nullptr;
		};

		struct PhysicsPanelContext
		{
			bool* isOpen = nullptr;
			bool* isPlaying = nullptr;
			PhysicsSystem* physicsSystem = nullptr;
			std::function<void(bool)> setPlaying;
		};

		struct CombatDebugPanelContext
		{
			bool* isOpen = nullptr;
			bool* drawCombatShapesOverlay = nullptr;
			::Scene* scene = nullptr;
			glm::mat4 viewMatrix = glm::mat4(1.0f);
			glm::mat4 projectionMatrix = glm::mat4(1.0f);
			int viewportWidth = 0;
			int viewportHeight = 0;
		};
	}
}
