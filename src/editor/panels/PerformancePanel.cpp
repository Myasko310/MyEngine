#include "PerformancePanel.h"

#ifdef USE_IMGUI
#include <glad/glad.h>

#include <algorithm>
#include <iostream>

#include "core/AssetManager.h"
#include "core/Input.h"
#include "components/CameraComponent.h"
#include "components/LightComponent.h"
#include "components/MeshRendererComponent.h"
#include "ecs/Scene.h"
#include "imgui.h"
#include "plugins/PluginManager.h"
#include "systems/MeshRendererSystem.h"
#include "systems/PhysicsSystem.h"

namespace MyEngine::Editor::Panels
{
	float FramePacingDiagnostics::GetChronologicalSample(int orderedIndex) const
	{
		if (sampleCount <= 0)
			return 0.0f;

		const int oldestIndex = (sampleCount < kSampleWindow) ? 0 : nextSampleIndex;
		const int sampleIndex = (oldestIndex + orderedIndex) % kSampleWindow;
		return frameMsSamples[sampleIndex];
	}

	void FramePacingDiagnostics::RecalculateRollingStats()
	{
		if (sampleCount <= 0)
		{
			averageFrameMs = 0.0f;
			minFrameMs = 0.0f;
			maxFrameMs = 0.0f;
			stdDevFrameMs = 0.0f;
			averageJitterMs = 0.0f;
			spikeCountOver16Ms = 0;
			spikeCountOver33Ms = 0;
			return;
		}

		float totalMs = 0.0f;
		float totalSqMs = 0.0f;
		float totalJitter = 0.0f;
		minFrameMs = GetChronologicalSample(0);
		maxFrameMs = minFrameMs;
		spikeCountOver16Ms = 0;
		spikeCountOver33Ms = 0;

		float previous = minFrameMs;
		for (int i = 0; i < sampleCount; ++i)
		{
			const float sample = GetChronologicalSample(i);
			totalMs += sample;
			totalSqMs += sample * sample;
			minFrameMs = std::min(minFrameMs, sample);
			maxFrameMs = std::max(maxFrameMs, sample);

			if (sample > 16.67f)
				++spikeCountOver16Ms;
			if (sample > 33.33f)
				++spikeCountOver33Ms;

			if (i > 0)
				totalJitter += std::abs(sample - previous);
			previous = sample;
		}

		averageFrameMs = totalMs / static_cast<float>(sampleCount);
		const float variance = std::max(0.0f, (totalSqMs / static_cast<float>(sampleCount)) - (averageFrameMs * averageFrameMs));
		stdDevFrameMs = std::sqrt(variance);
		averageJitterMs = (sampleCount > 1)
			? (totalJitter / static_cast<float>(sampleCount - 1))
			: 0.0f;
	}

	void DrawPerformancePanel(PerformancePanelContext& context)
	{
		if (!context.isOpen || !*context.isOpen || !context.scene || !context.renderSystem || !context.physicsSystem || !context.networkTransport || !context.pluginManager || !context.framePacingDiagnostics || !context.wireframe)
			return;

		auto& scene = *context.scene;
		auto& renderSystem = *context.renderSystem;
		auto& physicsSystem = *context.physicsSystem;
		auto& framePacingDiagnostics = *context.framePacingDiagnostics;
		auto& networkTransport = *context.networkTransport;
		auto& pluginManager = *context.pluginManager;

		ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
		ImGui::Begin("Performance", context.isOpen);

		float fps = 1.0f / std::max(0.0001f, context.deltaTime);
		ImGui::Text("FPS: %.1f", fps);
		ImGui::Text("CPU Frame: %.3f ms", context.deltaTime * 1000.0f);
		ImGui::Text("GPU Frame: %.3f ms", context.gpuFrameMs);
		ImGui::Text("CPU Physics: %.3f ms", context.cpuPhysicsMs);
		ImGui::Text("Replay: %s  Seed: %u  Frames: %zu  PlayIdx: %zu",
			Input::IsInputPlayback() ? "Playback" : (Input::IsInputRecording() ? "Recording" : "Live"),
			Input::GetReplaySeed(),
			Input::GetReplayFrameCount(),
			Input::GetReplayPlaybackIndex());
		ImGui::Text("Replay Sim: dt=%.3f  substeps=%d  particleSeed=%u",
			physicsSystem.fixedTimestep,
			physicsSystem.maxSubsteps,
			context.replayParticleSeed);
		ImGui::Text("CPU Animation+Particles: %.3f ms", context.cpuAnimationMs);
		ImGui::Text("CPU Render: %.3f ms", context.cpuRenderMs);
		ImGui::Separator();
		ImGui::Text("Frame Pacing Diagnostics");
		ImGui::Text("Window: %d frames", framePacingDiagnostics.sampleCount);
		ImGui::Text("Avg/Min/Max: %.3f / %.3f / %.3f ms",
			framePacingDiagnostics.averageFrameMs,
			framePacingDiagnostics.minFrameMs,
			framePacingDiagnostics.maxFrameMs);
		ImGui::Text("StdDev: %.3f ms  Avg Jitter: %.3f ms",
			framePacingDiagnostics.stdDevFrameMs,
			framePacingDiagnostics.averageJitterMs);
		ImGui::Text("Spikes >16.67ms: %d  >33.33ms: %d",
			framePacingDiagnostics.spikeCountOver16Ms,
			framePacingDiagnostics.spikeCountOver33Ms);
		ImGui::Text("Camera Step/Jitter: %.3f / %.3f u/s",
			framePacingDiagnostics.cameraStepPerSec,
			framePacingDiagnostics.cameraStepJitterPerSec);
		ImGui::Text("World Step/Jitter: %.3f / %.3f u/s",
			framePacingDiagnostics.worldStepPerSec,
			framePacingDiagnostics.worldStepJitterPerSec);
		ImGui::Text("Large Steps (Cam/World): %d / %d",
			framePacingDiagnostics.cameraLargeStepCount,
			framePacingDiagnostics.worldLargeStepCount);
		ImGui::Separator();
		ImGui::Text("Terrain/Nav Pipeline");
		ImGui::Text("Patch Queue: %zu (peak %zu)", context.pendingTerrainPatchJobCount, context.terrainPatchQueuePeak);
		ImGui::Text("Patch Prep: %.3f ms  Upload: %.3f ms", context.terrainPatchPrepMs, context.terrainPatchUploadMs);
		ImGui::Text("Backpressure Deferrals: %zu", context.terrainPatchBackpressureDeferrals);
		if (context.terrainPatchMaxQueuedJobs)
			ImGui::SliderInt("Max Pending Terrain Patch Jobs", context.terrainPatchMaxQueuedJobs, 1, 32);
		ImGui::Text("NavMesh Rebuild: %.3f ms (%s)", context.navMeshRebuildMs, context.navMeshLastRebuildWasRegion ? "Region" : "Full");
		bool occlusionApprox = renderSystem.GetOcclusionApproximationEnabled();
		if (ImGui::Checkbox("CPU Occlusion Approximation", &occlusionApprox))
			renderSystem.SetOcclusionApproximationEnabled(occlusionApprox);
		bool gpuOcclusionQueries = renderSystem.GetGPUOcclusionQueriesEnabled();
		if (ImGui::Checkbox("GPU Occlusion Queries", &gpuOcclusionQueries))
			renderSystem.SetGPUOcclusionQueriesEnabled(gpuOcclusionQueries);
		int queryRecheckFrames = renderSystem.GetOcclusionQueryRecheckFrames();
		if (ImGui::SliderInt("Occlusion Recheck Frames", &queryRecheckFrames, 1, 30))
			renderSystem.SetOcclusionQueryRecheckFrames(queryRecheckFrames);
		auto occDiag = renderSystem.GetOcclusionDiagnostics();
		ImGui::Text("Occlusion Visible: %d / %d", occDiag.visible, occDiag.totalCandidates);
		ImGui::Text("Frustum Reject: %d  Occlusion Reject: %d", occDiag.frustumRejected, occDiag.occlusionRejected);
		ImGui::Text("Temporal Reject: %d", occDiag.temporalRejected);
		ImGui::Text("Query Submitted: %d  Visible: %d  Hidden: %d", occDiag.querySubmitted, occDiag.queryVisible, occDiag.queryHidden);
#ifdef _WIN32
		ImGui::Text("Memory Working Set: %.1f MB", context.memoryWorkingSetMB);
		ImGui::Text("Memory Private: %.1f MB", context.memoryPrivateMB);
#endif

		ImGui::PlotLines("Frame (ms)", context.frameMsHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 50.0f, ImVec2(0, 70));
		ImGui::PlotLines("Physics (ms)", context.physicsMsHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 20.0f, ImVec2(0, 50));
		ImGui::PlotLines("Animation (ms)", context.animationMsHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 20.0f, ImVec2(0, 50));
		ImGui::PlotLines("Render (ms)", context.renderMsHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 30.0f, ImVec2(0, 50));
		ImGui::PlotLines("Visible Meshes", context.visibleCountHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 500.0f, ImVec2(0, 50));
		ImGui::PlotLines("Occlusion Rejects", context.occlusionRejectHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 500.0f, ImVec2(0, 50));
		ImGui::PlotLines("Frustum Rejects", context.frustumRejectHistory, static_cast<int>(context.perfHistoryCount), context.perfHistoryIndex, nullptr, 0.0f, 500.0f, ImVec2(0, 50));

		ImGui::Separator();
		ImGui::Text("Networking");
		const bool sessionReady = networkTransport.IsSessionReady();
		const bool sessionTimedOut = networkTransport.IsSessionTimedOut();
		const char* sessionStatus = sessionReady
			? "Ready"
			: (sessionTimedOut ? "Timed Out" : (context.networkSessionBootstrapPending ? "Connecting" : "Disconnected"));
		ImGui::Text("Session: %s", sessionStatus);
		ImGui::Text("Transport: %s", context.usingSocketTransport ? "Socket (UDP localhost)" : "InMemory Fallback");
		ImGui::Text("Client ID: %u", static_cast<unsigned int>(context.localClientID));
		ImGui::Text("Net Tick: %u", static_cast<unsigned int>(context.networkTick));
		ImGui::Text("Authoritative Scene: %s", context.hasAuthoritativeServerScene ? "Server/Client Split" : "Single Scene");
		ImGui::Text("Connect Retries: %u", static_cast<unsigned int>(networkTransport.GetConnectRetryCount()));
		ImGui::TextDisabled("Networking Mode: Hard-disabled in this build");
		ImGui::BeginDisabled(true);
		ImGui::Button("Reconnect Session");
		ImGui::SameLine();
		ImGui::Button("Disconnect Session");
		ImGui::EndDisabled();

		auto retrySettings = networkTransport.GetSessionRetrySettings();
		bool retryEnabled = retrySettings.enabled;
		if (ImGui::Checkbox("Connect Retry Enabled", &retryEnabled))
		{
			retrySettings.enabled = retryEnabled;
			networkTransport.SetSessionRetrySettings(retrySettings);
		}
		int connectTimeoutTicks = static_cast<int>(retrySettings.connectTimeoutTicks);
		if (ImGui::SliderInt("Connect Timeout (ticks)", &connectTimeoutTicks, 1, 120))
		{
			retrySettings.connectTimeoutTicks = static_cast<Net::NetTick>(std::max(connectTimeoutTicks, 1));
			networkTransport.SetSessionRetrySettings(retrySettings);
		}
		int retryIntervalTicks = static_cast<int>(retrySettings.retryIntervalTicks);
		if (ImGui::SliderInt("Retry Interval (ticks)", &retryIntervalTicks, 1, 60))
		{
			retrySettings.retryIntervalTicks = static_cast<Net::NetTick>(std::max(retryIntervalTicks, 1));
			networkTransport.SetSessionRetrySettings(retrySettings);
		}
		int maxRetries = static_cast<int>(retrySettings.maxRetries);
		if (ImGui::SliderInt("Max Connect Retries", &maxRetries, 0, 10))
		{
			retrySettings.maxRetries = static_cast<std::uint32_t>(std::max(maxRetries, 0));
			networkTransport.SetSessionRetrySettings(retrySettings);
		}

		auto simSettings = networkTransport.GetSimulationSettings();
		bool simEnabled = simSettings.enabled;
		if (ImGui::Checkbox("Simulate Packet Conditions", &simEnabled))
		{
			simSettings.enabled = simEnabled;
			networkTransport.SetSimulationSettings(simSettings);
		}

		float lossPercent = simSettings.packetLossChance * 100.0f;
		if (ImGui::SliderFloat("Packet Loss %", &lossPercent, 0.0f, 100.0f, "%.1f"))
		{
			simSettings.packetLossChance = std::clamp(lossPercent / 100.0f, 0.0f, 1.0f);
			networkTransport.SetSimulationSettings(simSettings);
		}

		int jitterMin = static_cast<int>(simSettings.jitterMinTicks);
		int jitterMax = static_cast<int>(simSettings.jitterMaxTicks);
		if (ImGui::SliderInt("Jitter Min (ticks)", &jitterMin, 0, 30))
		{
			simSettings.jitterMinTicks = static_cast<Net::NetTick>(std::max(jitterMin, 0));
			networkTransport.SetSimulationSettings(simSettings);
		}
		if (ImGui::SliderInt("Jitter Max (ticks)", &jitterMax, 0, 30))
		{
			simSettings.jitterMaxTicks = static_cast<Net::NetTick>(std::max(jitterMax, 0));
			networkTransport.SetSimulationSettings(simSettings);
		}

		float reorderPercent = simSettings.reorderChance * 100.0f;
		if (ImGui::SliderFloat("Reorder %", &reorderPercent, 0.0f, 100.0f, "%.1f"))
		{
			simSettings.reorderChance = std::clamp(reorderPercent / 100.0f, 0.0f, 1.0f);
			networkTransport.SetSimulationSettings(simSettings);
		}

		if (context.transportInputDelayTicks)
		{
			int inputDelayTicks = static_cast<int>(*context.transportInputDelayTicks);
			if (ImGui::SliderInt("Input Delay (ticks)", &inputDelayTicks, 0, 10))
				*context.transportInputDelayTicks = static_cast<Net::NetTick>(std::max(inputDelayTicks, 0));
		}

		if (context.transportSnapshotDelayTicks)
		{
			int snapshotDelayTicks = static_cast<int>(*context.transportSnapshotDelayTicks);
			if (ImGui::SliderInt("Snapshot Delay (ticks)", &snapshotDelayTicks, 0, 20))
				*context.transportSnapshotDelayTicks = static_cast<Net::NetTick>(std::max(snapshotDelayTicks, 0));
		}

		if (context.networkClientSmoothingEnabled)
			ImGui::Checkbox("Client Smoothing Enabled", context.networkClientSmoothingEnabled);
		if (context.networkClientPositionSmoothingRate)
			ImGui::SliderFloat("Client Position Smooth Rate", context.networkClientPositionSmoothingRate, 1.0f, 30.0f, "%.1f");
		if (context.networkClientRotationSmoothingRate)
			ImGui::SliderFloat("Client Rotation Smooth Rate", context.networkClientRotationSmoothingRate, 1.0f, 30.0f, "%.1f");
		if (context.networkClientPositionSnapDistance)
			ImGui::SliderFloat("Client Position Snap Distance", context.networkClientPositionSnapDistance, 0.05f, 10.0f, "%.2f");
		if (context.networkClientRotationSnapDistance)
			ImGui::SliderFloat("Client Rotation Snap Distance", context.networkClientRotationSnapDistance, 1.0f, 180.0f, "%.1f");

		if (context.networkInterestSettings)
		{
			ImGui::Checkbox("Interest Filter Enabled", &context.networkInterestSettings->enabled);
			ImGui::SliderFloat("Interest Radius", &context.networkInterestSettings->radius, 1.0f, 200.0f, "%.1f");
		}

		ImGui::Separator();
		ImGui::Text("Texture Streaming");
		bool textureStreamingEnabled = AssetManager::GetTextureStreamingEnabled();
		if (ImGui::Checkbox("Enable Texture Streaming", &textureStreamingEnabled))
			AssetManager::SetTextureStreamingEnabled(textureStreamingEnabled);
		int streamingQuality = static_cast<int>(AssetManager::GetTextureStreamingQuality());
		const char* streamingOptions[] = { "Full Resolution", "Half Resolution", "Quarter Resolution" };
		if (ImGui::Combo("Streaming Quality", &streamingQuality, streamingOptions, IM_ARRAYSIZE(streamingOptions)))
			AssetManager::SetTextureStreamingQuality(static_cast<AssetManager::TextureStreamingQuality>(streamingQuality));

		ImGui::Separator();
		ImGui::Text("Shader Hot Reload");
		bool shaderAutoReload = AssetManager::GetShaderAutoHotReloadEnabled();
		if (ImGui::Checkbox("Auto Shader Reload", &shaderAutoReload))
			AssetManager::SetShaderAutoHotReloadEnabled(shaderAutoReload);
		if (ImGui::Button("Reload All Shaders"))
		{
			bool ok = AssetManager::ReloadAllShaders(false);
			if (!ok)
				std::cerr << "Shader reload reported errors" << std::endl;
		}
		auto shaderErrors = AssetManager::GetShaderErrorReport();
		if (!shaderErrors.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "Shader Errors:");
			for (const auto& err : shaderErrors)
				ImGui::BulletText("%s", err.c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Shader status: OK");
		}

		ImGui::Separator();
		ImGui::Text("Plugins");
		ImGui::Text("Loaded: %zu", pluginManager.GetLoadedPlugins().size());
		if (ImGui::Button("Load Plugins") && context.loadPlugins)
			context.loadPlugins();
		ImGui::SameLine();
		if (ImGui::Button("Reload Plugins (F10)") && context.reloadPlugins)
			context.reloadPlugins();
		ImGui::SameLine();
		if (ImGui::Button("Unload Plugins") && context.unloadPlugins)
			context.unloadPlugins();

		for (const auto& loadedPlugin : pluginManager.GetLoadedPlugins())
			ImGui::BulletText("%s", loadedPlugin.manifest.packageName.c_str());

		const auto& pluginErrors = pluginManager.GetLastErrors();
		if (!pluginErrors.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Plugin Loader Messages:");
			for (const auto& pluginError : pluginErrors)
				ImGui::BulletText("%s", pluginError.c_str());
		}

		if (ImGui::Button("Export CSV##perfExport") && context.exportPerformanceCsv)
		{
			if (!context.exportPerformanceCsv())
				std::cerr << "Failed to export performance_metrics.csv" << std::endl;
		}

		ImGui::Separator();
		bool mouseCaptured = Input::IsMouseCaptured();
		if (mouseCaptured)
		{
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Mouse: CAMERA CONTROL");
			ImGui::Text("(Right-click to interact with UI)");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Mouse: UI INTERACTION");
			ImGui::Text("(Right-click for camera control)");
		}

		ImGui::Separator();
		ImGui::Text("Scene Stats:");
		ImGui::Text("  Entities: %zu", scene.GetEntities().size());

		int meshCount = 0;
		int lightCount = 0;
		int cameraCount = 0;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity->HasComponent<::MeshRendererComponent>()) ++meshCount;
			if (entity->HasComponent<LightComponent>()) ++lightCount;
			if (entity->HasComponent<CameraComponent>()) ++cameraCount;
		}

		ImGui::Text("  Meshes: %d", meshCount);
		ImGui::Text("  Lights: %d", lightCount);
		ImGui::Text("  Cameras: %d", cameraCount);

		ImGui::Separator();
		ImGui::Text("Viewport: %dx%d", context.viewportWidth, context.viewportHeight);
		ImGui::Checkbox("Wireframe", context.wireframe);
		renderSystem.SetWireframe(*context.wireframe);

		int debugViewMode = static_cast<int>(renderSystem.GetDebugViewMode());
		const char* debugViewOptions[] = {
			"Final Lit",
			"Albedo",
			"Normal",
			"Roughness",
			"Metallic",
			"AO",
			"Emissive",
			"Shadow",
			"SSAO"
		};
		if (ImGui::Combo("Debug View", &debugViewMode, debugViewOptions, IM_ARRAYSIZE(debugViewOptions)))
			renderSystem.SetDebugViewMode(static_cast<MeshRendererSystem::DebugViewMode>(debugViewMode));

		ImGui::End();
	}
}
#endif
