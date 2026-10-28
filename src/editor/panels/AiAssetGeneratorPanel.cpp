#include "AiAssetGeneratorPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <rapidjson/document.h>

#include <vector>

#include "ai/AiAssetGeneration.h"
#include "core/AssetPipeline.h"
#include "editor/EditorStyle.h"
#include "editor/PanelDockManager.h"

namespace MyEngine::Editor::Panels
{
	namespace
	{
		bool OpenPathWithShell(const std::string& path)
		{
#if defined(_WIN32)
			const std::string command = "start \"\" \"" + path + "\"";
			return std::system(command.c_str()) == 0;
#else
			(const std::string&)path;
			return false;
#endif
		}

		std::filesystem::path ResolvePathFromCurrentOrParents(const std::filesystem::path& path)
		{
			if (path.empty())
				return {};
			if (path.is_absolute() && std::filesystem::exists(path))
				return path;

			std::error_code ec;
			const std::filesystem::path cwd = std::filesystem::current_path(ec);
			if (ec)
				return path;

			for (std::filesystem::path cursor = cwd; !cursor.empty(); cursor = cursor.parent_path())
			{
				const std::filesystem::path candidate = cursor / path;
				if (std::filesystem::exists(candidate, ec))
					return candidate;
				if (cursor == cursor.parent_path())
					break;
			}

			return path;
		}

		bool RunCommandCapture(const std::string& command, std::string& output, int& outExitCode)
		{
			output.clear();
			std::array<char, 512> buffer{};
			FILE* pipe = _popen(command.c_str(), "r");
			if (!pipe)
			{
				outExitCode = -1;
				return false;
			}

			while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
				output += buffer.data();

			outExitCode = _pclose(pipe);
			return true;
		}

		struct BackendHealthSummary
		{
			bool available = false;
			bool success = false;
			std::string mode;
			int templateCount = -1;
			int durationMs = -1;
			bool hasRepoEnv = false;
			bool repoExists = false;
			bool hasExplicitTemplate = false;
			std::string outputGlb;
			std::string message;
		};

		BackendHealthSummary LoadBackendHealthSummary(const std::filesystem::path& healthPath)
		{
			BackendHealthSummary summary;
			std::error_code ec;
			if (!std::filesystem::exists(healthPath, ec))
				return summary;

			std::ifstream ifs(healthPath);
			if (!ifs.is_open())
				return summary;

			std::string json((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			rapidjson::Document doc;
			doc.Parse(json.c_str());
			if (doc.HasParseError() || !doc.IsObject())
				return summary;

			summary.available = true;
			if (doc.HasMember("success") && doc["success"].IsBool())
				summary.success = doc["success"].GetBool();
			if (doc.HasMember("mode") && doc["mode"].IsString())
				summary.mode = doc["mode"].GetString();
			if (doc.HasMember("template_count") && doc["template_count"].IsInt())
				summary.templateCount = doc["template_count"].GetInt();
			if (doc.HasMember("duration_ms") && doc["duration_ms"].IsInt())
				summary.durationMs = doc["duration_ms"].GetInt();
			if (doc.HasMember("has_repo_env") && doc["has_repo_env"].IsBool())
				summary.hasRepoEnv = doc["has_repo_env"].GetBool();
			if (doc.HasMember("repo_exists") && doc["repo_exists"].IsBool())
				summary.repoExists = doc["repo_exists"].GetBool();
			if (doc.HasMember("has_explicit_template") && doc["has_explicit_template"].IsBool())
				summary.hasExplicitTemplate = doc["has_explicit_template"].GetBool();
			if (doc.HasMember("output_glb") && doc["output_glb"].IsString())
				summary.outputGlb = doc["output_glb"].GetString();
			if (doc.HasMember("message") && doc["message"].IsString())
				summary.message = doc["message"].GetString();

			return summary;
		}

		bool ProbeComfyUiEndpoint(const std::string& baseUrl, std::string& outMessage)
		{
			if (baseUrl.empty())
			{
				outMessage = "ComfyUI endpoint URL is empty.";
				return false;
			}

			const std::string command = "curl -sS -m 2 \"" + baseUrl + "/system_stats\"";
			std::array<char, 256> buffer{};
			std::string output;

			FILE* pipe = _popen(command.c_str(), "r");
			if (!pipe)
			{
				outMessage = "Unable to launch curl for ComfyUI probe.";
				return false;
			}

			while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
				output += buffer.data();

			const int code = _pclose(pipe);
			if (code != 0)
			{
				outMessage = "ComfyUI probe request failed.";
				return false;
			}

			if (output.find("system") == std::string::npos && output.find("devices") == std::string::npos)
			{
				outMessage = "ComfyUI endpoint responded with unexpected payload.";
				return false;
			}

			outMessage = "ComfyUI is reachable.";
			return true;
		}
	}

	void DrawAiAssetGeneratorPanel(Context& context)
	{
		if (!context.ui)
			return;

		auto& ui = *context.ui;
		static bool panelWasVisibleLastFrame = false;
		if (!ui.showAiAssetGeneratorPanel)
		{
			panelWasVisibleLastFrame = false;
			return;
		}

		PanelDockManager::Instance().SnapPanel("AI Asset Generator");
		ImGui::Begin("AI Asset Generator", &ui.showAiAssetGeneratorPanel, MyEngine::Editor::DockablePanelFlags);

		ImGui::TextWrapped("Describe a prop and generate AI assets locally. Preferred flow uses real text-to-3D GLB generation, then queues outputs under assets/generated/ai for import.");
		ImGui::Spacing();

		static char promptBuffer[512] = "Stone temple pillar with worn edges and carved runes";
		static char outputNameBuffer[128] = "temple_pillar";
		static int lodCount = 3;
		static int textureResolution = 512;
		static bool generateCollision = true;
		static char textTo3dPythonExeBuffer[128] = "python";
		static char textTo3dRunnerPathBuffer[256] = "tools/ai/generate_text_to_3d.py";
		static char textTo3dBackendBuffer[64] = "hunyuan3d";
		static int textTo3dTimeoutMs = 300000;
		static int textTo3dRetryCount = 1;
		static int textTo3dRetryBackoffMs = 250;
		static bool textTo3dRequireGlb = true;
		static std::vector<int> backendDurationHistoryMs;
		static std::vector<int> backendAttemptHistory;
		static char comfyEndpointBuffer[256] = "http://127.0.0.1:8188";
		static char comfyWorkflowPathBuffer[256] = "assets/ai/comfyui_workflow_api.json";
		static char comfyPromptTokenBuffer[64] = "{{PROMPT}}";
		static int comfyPollIntervalMs = 1000;
		static int comfyPollTimeoutMs = 120000;
		static std::string statusMessage;
		static bool lastGenerationSucceeded = false;
		static std::string preflightReport;
		static std::string lastGeneratedAssetDirectory;
		static std::string lastGeneratedPrimaryModelPath;
		static std::string comfyAvailabilityMessage = "ComfyUI status unknown.";
		static bool comfyIsAvailable = false;
		static bool comfyProbeHasResult = false;
		static std::string lastProbedEndpoint;
		static std::chrono::steady_clock::time_point nextProbeTime = std::chrono::steady_clock::time_point::min();
		static bool providerAutoSelectionApplied = false;
		static std::string lastAutoPreflightKey;

		auto& generationService = MyEngine::AI::AiAssetGenerationService::Get();
		std::vector<std::string> providerIds = generationService.GetProviderIds();
		if (providerIds.empty())
			providerIds.push_back("none");

		if (!providerAutoSelectionApplied)
		{
			auto preferredIt = std::find(providerIds.begin(), providerIds.end(), "local_text_to_3d_glb_v1");
			if (preferredIt != providerIds.end())
				generationService.SetActiveProvider("local_text_to_3d_glb_v1");
			providerAutoSelectionApplied = true;
		}

		int selectedProviderIndex = 0;
		for (std::size_t i = 0; i < providerIds.size(); ++i)
		{
			if (providerIds[i] == generationService.GetActiveProviderId())
			{
				selectedProviderIndex = static_cast<int>(i);
				break;
			}
		}

		std::string providerList;
		for (const auto& provider : providerIds)
		{
			providerList += provider;
			providerList.push_back('\0');
		}
		providerList.push_back('\0');

		InspectorGroupLabel("Generator Settings");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("Provider", &selectedProviderIndex, providerList.c_str()))
			generationService.SetActiveProvider(providerIds[static_cast<std::size_t>(selectedProviderIndex)]);

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##aiPrompt", "Describe the object to generate", promptBuffer, sizeof(promptBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##aiOutputName", "Output asset name (optional)", outputNameBuffer, sizeof(outputNameBuffer));
		ImGui::SliderInt("LOD Count", &lodCount, 1, 4);
		ImGui::SliderInt("Texture Resolution", &textureResolution, 128, 2048, "%d", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("Generate Collision Mesh", &generateCollision);

		InspectorGroupLabel("Real Text-to-3D GLB Runner");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##textTo3dPythonExe", "Python executable", textTo3dPythonExeBuffer, sizeof(textTo3dPythonExeBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##textTo3dRunnerPath", "Runner script path", textTo3dRunnerPathBuffer, sizeof(textTo3dRunnerPathBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##textTo3dBackend", "Backend id", textTo3dBackendBuffer, sizeof(textTo3dBackendBuffer));
		ImGui::SliderInt("Text-to-3D Timeout (ms)", &textTo3dTimeoutMs, 1000, 600000);
		ImGui::SliderInt("Retry Count", &textTo3dRetryCount, 1, 5);
		ImGui::SliderInt("Retry Backoff (ms)", &textTo3dRetryBackoffMs, 0, 5000);
		ImGui::Checkbox("Require GLB Output", &textTo3dRequireGlb);

		const char* textTo3dCommandEnv = std::getenv("MYENGINE_TEXT_TO_3D_COMMAND");
		const bool hasTextTo3dCommandEnv = textTo3dCommandEnv && textTo3dCommandEnv[0] != '\0';
		const std::filesystem::path defaultHunyuanTemplatePath = ResolvePathFromCurrentOrParents("tools/ai/hunyuan3d_command_template.ps1");
		std::error_code defaultTemplateEc;
		const bool hasDefaultHunyuanTemplate = std::filesystem::exists(defaultHunyuanTemplatePath, defaultTemplateEc);
		if (!hasTextTo3dCommandEnv)
		{
			ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.0f), "MYENGINE_TEXT_TO_3D_COMMAND is not set.");
			if (hasDefaultHunyuanTemplate)
				ImGui::TextDisabled("Using bundled default template fallback: %s", defaultHunyuanTemplatePath.generic_string().c_str());
			else
				ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.35f, 1.0f), "Default fallback template is also missing.");
		}

		const char* hunyuanScriptEnv = std::getenv("HUNYUAN3D_SCRIPT");
		const bool hasHunyuanScriptEnv = hunyuanScriptEnv && hunyuanScriptEnv[0] != '\0';
		const std::string hunyuanScriptPath = hasHunyuanScriptEnv ? std::string(hunyuanScriptEnv) : std::string("tools/hunyuan3d/infer_text_to_3d.py");
		std::error_code hunyuanScriptEc;
		const bool hunyuanScriptExists = std::filesystem::exists(hunyuanScriptPath, hunyuanScriptEc);
		const char* hunyuanRepoEnv = std::getenv("HUNYUAN3D_REPO");
		const bool hasHunyuanRepoEnv = hunyuanRepoEnv && hunyuanRepoEnv[0] != '\0';
		const char* hunyuanInferTemplateEnv = std::getenv("HUNYUAN3D_INFER_COMMAND_TEMPLATE");
		const bool hasHunyuanInferTemplateEnv = hunyuanInferTemplateEnv && hunyuanInferTemplateEnv[0] != '\0';

		if (!hasHunyuanScriptEnv)
		{
			ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.0f), "HUNYUAN3D_SCRIPT is not set.");
			ImGui::TextDisabled("Using fallback path: %s", hunyuanScriptPath.c_str());
			if (InspectorActionButton("Copy HUNYUAN3D_SCRIPT Template"))
			{
				ImGui::SetClipboardText("$env:HUNYUAN3D_SCRIPT = \"C:\\\\path\\\\to\\\\hunyuan3d\\\\infer_text_to_3d.py\"");
				statusMessage = "Copied HUNYUAN3D_SCRIPT template to clipboard.";
				lastGenerationSucceeded = true;
			}
		}
		if (!hasHunyuanRepoEnv && !hasHunyuanInferTemplateEnv)
		{
			ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.0f), "Backend bridge is missing execution config.");
			ImGui::TextDisabled("Set HUNYUAN3D_REPO or HUNYUAN3D_INFER_COMMAND_TEMPLATE.");
		}
		if (!hunyuanScriptExists)
		{
			ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.35f, 1.0f), "Configured Hunyuan3D script path not found.");
			ImGui::TextDisabled("Missing path: %s", hunyuanScriptPath.c_str());
			if (InspectorActionButton("Copy Absolute Script Path Hint"))
			{
				ImGui::SetClipboardText("setx HUNYUAN3D_SCRIPT \"C:\\\\absolute\\\\path\\\\to\\\\infer_text_to_3d.py\"");
				statusMessage = "Copied HUNYUAN3D_SCRIPT setx hint to clipboard.";
				lastGenerationSucceeded = true;
			}
		}

		InspectorGroupLabel("ComfyUI (Local Backend)");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##comfyEndpoint", "ComfyUI API Base URL", comfyEndpointBuffer, sizeof(comfyEndpointBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##comfyWorkflow", "Workflow template path", comfyWorkflowPathBuffer, sizeof(comfyWorkflowPathBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##comfyPromptToken", "Prompt token in workflow", comfyPromptTokenBuffer, sizeof(comfyPromptTokenBuffer));
		ImGui::SliderInt("Comfy Poll Interval (ms)", &comfyPollIntervalMs, 100, 5000);
		ImGui::SliderInt("Comfy Timeout (ms)", &comfyPollTimeoutMs, 1000, 300000);

		const std::filesystem::path backendHealthSummaryPath = ResolvePathFromCurrentOrParents("tools/hunyuan3d/backend_health_summary.json");
		const BackendHealthSummary backendHealthSummary = LoadBackendHealthSummary(backendHealthSummaryPath);

		const std::string currentComfyEndpoint = comfyEndpointBuffer;
		const auto now = std::chrono::steady_clock::now();
		const bool endpointChanged = currentComfyEndpoint != lastProbedEndpoint;
		if ((endpointChanged || !comfyProbeHasResult) && now >= nextProbeTime)
		{
			comfyIsAvailable = ProbeComfyUiEndpoint(currentComfyEndpoint, comfyAvailabilityMessage);
			comfyProbeHasResult = true;
			lastProbedEndpoint = currentComfyEndpoint;
			nextProbeTime = now + std::chrono::seconds(5);
		}

		ImVec4 comfyStatusColor = ImVec4(1.00f, 0.75f, 0.20f, 1.0f);
		const char* comfyStatusLabel = "Unknown";
		if (comfyProbeHasResult)
		{
			comfyStatusColor = comfyIsAvailable ? ImVec4(0.30f, 1.00f, 0.50f, 1.0f) : ImVec4(1.00f, 0.45f, 0.35f, 1.0f);
			comfyStatusLabel = comfyIsAvailable ? "Online" : "Offline";
		}
		ImGui::TextColored(comfyStatusColor, "ComfyUI Status: %s", comfyStatusLabel);
		ImGui::TextDisabled("%s", comfyAvailabilityMessage.c_str());
		if (InspectorActionButton("Refresh ComfyUI Status"))
		{
			const std::string endpoint = comfyEndpointBuffer;
			comfyIsAvailable = ProbeComfyUiEndpoint(endpoint, comfyAvailabilityMessage);
			comfyProbeHasResult = true;
			lastProbedEndpoint = endpoint;
			nextProbeTime = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		}

		const std::string activeProviderId = generationService.GetActiveProviderId();
		const bool promptValid = promptBuffer[0] != '\0';
		std::error_code runnerPathEc;
		std::error_code workflowPathEc;
		const std::filesystem::path resolvedRunnerScriptPath = ResolvePathFromCurrentOrParents(textTo3dRunnerPathBuffer);
		const std::filesystem::path resolvedComfyWorkflowPath = ResolvePathFromCurrentOrParents(comfyWorkflowPathBuffer);
		const bool runnerScriptExists = std::filesystem::exists(resolvedRunnerScriptPath, runnerPathEc);
		const bool comfyWorkflowExists = std::filesystem::exists(resolvedComfyWorkflowPath, workflowPathEc);

		bool canGenerate = promptValid;
		std::string preflightIssue;
		if (!promptValid)
		{
			preflightIssue = "Prompt is required.";
		}
		else if (activeProviderId == "local_text_to_3d_glb_v1")
		{
			if (!hasTextTo3dCommandEnv && !hasDefaultHunyuanTemplate)
			{
				canGenerate = false;
				preflightIssue = "Set MYENGINE_TEXT_TO_3D_COMMAND or restore tools/ai/hunyuan3d_command_template.ps1 fallback.";
			}
			else if (!runnerScriptExists)
			{
				canGenerate = false;
				preflightIssue = "Runner script path does not exist: " + resolvedRunnerScriptPath.generic_string();
			}
			else if (!hasTextTo3dCommandEnv && !hasHunyuanRepoEnv && !hasHunyuanInferTemplateEnv)
			{
				canGenerate = false;
				preflightIssue = "Set HUNYUAN3D_REPO or HUNYUAN3D_INFER_COMMAND_TEMPLATE for backend bridge execution.";
			}
		}
		else if (activeProviderId == "comfyui_local_v1")
		{
			if (!comfyWorkflowExists)
			{
				canGenerate = false;
				preflightIssue = "ComfyUI workflow template path does not exist: " + resolvedComfyWorkflowPath.generic_string();
			}
			else if (comfyPromptTokenBuffer[0] == '\0')
			{
				canGenerate = false;
				preflightIssue = "Comfy prompt token is required for workflow replacement.";
			}
			else if (comfyProbeHasResult && !comfyIsAvailable)
			{
				canGenerate = false;
				preflightIssue = "ComfyUI endpoint appears offline. Check status or refresh before generating.";
			}
		}

		auto runPreflightDiagnostic = [&]()
		{
			std::ostringstream report;
			report << "Provider: " << activeProviderId << "\n";
			report << "Prompt: " << (promptValid ? "OK" : "Missing") << "\n";
			report << "MYENGINE_TEXT_TO_3D_COMMAND: " << (hasTextTo3dCommandEnv ? "Set" : "Missing") << "\n";
			report << "Default fallback template: " << defaultHunyuanTemplatePath.generic_string() << " (" << (hasDefaultHunyuanTemplate ? "Found" : "Missing") << ")\n";
			report << "Python executable: " << textTo3dPythonExeBuffer << "\n";
			report << "Runner script path (input): " << textTo3dRunnerPathBuffer << "\n";
			report << "Runner script path (resolved): " << resolvedRunnerScriptPath.generic_string() << " (" << (runnerScriptExists ? "Found" : "Missing") << ")\n";
			report << "HUNYUAN3D_SCRIPT env: " << (hasHunyuanScriptEnv ? "Set" : "Missing") << "\n";
			report << "Resolved Hunyuan script: " << hunyuanScriptPath << " (" << (hunyuanScriptExists ? "Found" : "Missing") << ")\n";
			report << "HUNYUAN3D_REPO: " << (hasHunyuanRepoEnv ? "Set" : "Missing") << "\n";
			report << "HUNYUAN3D_INFER_COMMAND_TEMPLATE: " << (hasHunyuanInferTemplateEnv ? "Set" : "Missing") << "\n";
			report << "Comfy workflow path (input): " << comfyWorkflowPathBuffer << "\n";
			report << "Comfy workflow path (resolved): " << resolvedComfyWorkflowPath.generic_string() << " (" << (comfyWorkflowExists ? "Found" : "Missing") << ")\n";
			report << "Comfy endpoint status: " << (comfyProbeHasResult ? (comfyIsAvailable ? "Online" : "Offline") : "Unknown") << "\n";
			if (!preflightIssue.empty())
				report << "Blocking issue: " << preflightIssue << "\n";
			else
				report << "Blocking issue: none\n";
			preflightReport = report.str();
		};

		ImVec4 readyColor = canGenerate ? ImVec4(0.30f, 1.00f, 0.50f, 1.0f) : ImVec4(1.00f, 0.45f, 0.35f, 1.0f);
		const std::string readyMessage = canGenerate
			? "Text-to-3D readiness: READY"
			: ("Text-to-3D readiness: NOT READY - " + preflightIssue);
		ImGui::TextColored(readyColor, "%s", readyMessage.c_str());

		const std::string autoPreflightKey =
			activeProviderId + "|" +
			std::string(promptBuffer) + "|" +
			std::string(textTo3dRunnerPathBuffer) + "|" +
			std::string(comfyWorkflowPathBuffer) + "|" +
			(currentComfyEndpoint) + "|" +
			(hasTextTo3dCommandEnv ? "1" : "0") + "|" +
			(hasDefaultHunyuanTemplate ? "1" : "0") + "|" +
			(runnerScriptExists ? "1" : "0") + "|" +
			(comfyWorkflowExists ? "1" : "0") + "|" +
			(comfyProbeHasResult ? (comfyIsAvailable ? "online" : "offline") : "unknown") + "|" +
			(hasHunyuanRepoEnv ? "1" : "0") + "|" +
			(hasHunyuanInferTemplateEnv ? "1" : "0");
		if (preflightReport.empty() || autoPreflightKey != lastAutoPreflightKey)
		{
			runPreflightDiagnostic();
			lastAutoPreflightKey = autoPreflightKey;
		}
		const bool panelOpenedThisFrame = !panelWasVisibleLastFrame;
		if (panelOpenedThisFrame && runnerScriptExists && activeProviderId == "local_text_to_3d_glb_v1")
		{
			const std::string pythonExe = textTo3dPythonExeBuffer;
			const std::string healthJsonPath = (resolvedRunnerScriptPath.parent_path() / ".." / "hunyuan3d" / "backend_health_summary.json").lexically_normal().generic_string();
			const std::string command = "call \"" + pythonExe + "\" \"" + hunyuanScriptPath + "\" --self_check --health_json \"" + healthJsonPath + "\"";
			std::string output;
			int exitCode = 0;
			if (RunCommandCapture(command, output, exitCode))
			{
				std::ostringstream report;
				report << preflightReport;
				report << "\nAuto backend self-check command: " << command << "\n";
				report << "Auto backend self-check exit code: " << exitCode << "\n";
				report << "Auto backend self-check output:\n" << output;
				preflightReport = report.str();
			}
		}
		panelWasVisibleLastFrame = true;

		if (InspectorActionButton("Run Preflight Test"))
		{
			runPreflightDiagnostic();
			statusMessage = canGenerate ? "Preflight passed." : "Preflight failed. See report.";
			lastGenerationSucceeded = canGenerate;
			lastAutoPreflightKey = autoPreflightKey;
		}

		ImGui::BeginDisabled(!runnerScriptExists);
		if (InspectorActionButton("Test Runner Command"))
		{
			const std::string pythonExe = textTo3dPythonExeBuffer;
			const std::string command = "call \"" + pythonExe + "\" \"" + resolvedRunnerScriptPath.generic_string() + "\" --help";
			std::string output;
			int exitCode = 0;
			if (!RunCommandCapture(command, output, exitCode))
			{
				statusMessage = "Failed to start runner command test process.";
				lastGenerationSucceeded = false;
			}
			else
			{
				std::ostringstream report;
				report << preflightReport;
				report << "\nRunner command test exit code: " << exitCode << "\n";
				report << "Runner command test output:\n" << output;
				preflightReport = report.str();
				statusMessage = exitCode == 0 ? "Runner command test passed." : "Runner command test failed. See preflight report.";
				lastGenerationSucceeded = exitCode == 0;
			}
		}
		if (InspectorActionButton("Test Backend Bridge"))
		{
			const std::string pythonExe = textTo3dPythonExeBuffer;
			const std::string healthJsonPath = (resolvedRunnerScriptPath.parent_path() / ".." / "hunyuan3d" / "backend_health_summary.json").lexically_normal().generic_string();
			const std::string command = "call \"" + pythonExe + "\" \"" + hunyuanScriptPath + "\" --self_check --health_json \"" + healthJsonPath + "\"";
			std::string output;
			int exitCode = 0;
			if (!RunCommandCapture(command, output, exitCode))
			{
				statusMessage = "Failed to start backend bridge self-check.";
				lastGenerationSucceeded = false;
			}
			else
			{
				std::ostringstream report;
				report << preflightReport;
				report << "\nBackend bridge self-check command: " << command << "\n";
				report << "Backend bridge self-check exit code: " << exitCode << "\n";
				report << "Backend bridge self-check output:\n" << output;
				preflightReport = report.str();
				statusMessage = exitCode == 0 ? "Backend bridge self-check passed." : "Backend bridge self-check failed. See preflight report.";
				lastGenerationSucceeded = exitCode == 0;
			}
		}
		ImGui::EndDisabled();
		if (!runnerScriptExists)
			ImGui::TextDisabled("Runner checks disabled until runner script path resolves.");

		if (activeProviderId == "local_text_to_3d_glb_v1")
		{
			if (!hasTextTo3dCommandEnv)
				ImGui::TextDisabled("Runner command source: default fallback template");
			else
				ImGui::TextDisabled("Runner command source: MYENGINE_TEXT_TO_3D_COMMAND");

			if (!hasHunyuanRepoEnv && !hasHunyuanInferTemplateEnv)
				ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.0f), "Backend bridge self-check is expected to fail until backend env is configured.");

			if (ImGui::CollapsingHeader("Backend Health Summary", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (!backendHealthSummary.available)
				{
					ImGui::TextDisabled("No backend health summary found yet. Run Test Backend Bridge.");
				}
				else
				{
					ImVec4 healthColor = backendHealthSummary.success ? ImVec4(0.30f, 1.00f, 0.50f, 1.0f) : ImVec4(1.00f, 0.45f, 0.35f, 1.0f);
					ImGui::TextColored(healthColor, "Status: %s", backendHealthSummary.success ? "Healthy" : "Issue Detected");
					ImGui::TextDisabled("Mode: %s", backendHealthSummary.mode.empty() ? "n/a" : backendHealthSummary.mode.c_str());
					ImGui::TextDisabled("Template Count: %d", backendHealthSummary.templateCount);
					if (backendHealthSummary.durationMs >= 0)
						ImGui::TextDisabled("Check Duration: %d ms", backendHealthSummary.durationMs);
					if (!backendDurationHistoryMs.empty())
					{
						const int latestDuration = backendDurationHistoryMs.back();
						const int latestAttempts = backendAttemptHistory.empty() ? 0 : backendAttemptHistory.back();
						ImGui::TextDisabled("Last Generation Duration: %d ms", latestDuration);
						ImGui::TextDisabled("Last Generation Attempts: %d", latestAttempts);
					}
					ImGui::TextDisabled("Repo Env: %s", backendHealthSummary.hasRepoEnv ? "Set" : "Missing");
					ImGui::TextDisabled("Repo Exists: %s", backendHealthSummary.repoExists ? "Yes" : "No");
					ImGui::TextDisabled("Explicit Template Env: %s", backendHealthSummary.hasExplicitTemplate ? "Set" : "Missing");
					if (!backendHealthSummary.outputGlb.empty())
						ImGui::TextDisabled("Output GLB: %s", backendHealthSummary.outputGlb.c_str());
					if (!backendHealthSummary.message.empty())
						ImGui::TextWrapped("%s", backendHealthSummary.message.c_str());
				}
			}
		}

		if (!preflightReport.empty() && ImGui::CollapsingHeader("Preflight Report", ImGuiTreeNodeFlags_DefaultOpen))
			ImGui::TextWrapped("%s", preflightReport.c_str());

		if (!canGenerate)
		{
			ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.35f, 1.0f), "%s", preflightIssue.c_str());
			if (InspectorActionButton("Open Setup Guide"))
			{
				if (!OpenPathWithShell("docs/ai-text-to-3d-setup.md"))
					statusMessage = "Failed to open setup guide. Open docs/ai-text-to-3d-setup.md manually.";
			}
			if (activeProviderId == "local_text_to_3d_glb_v1" && !hasTextTo3dCommandEnv && !hasDefaultHunyuanTemplate)
			{
				if (InspectorActionButton("Copy Env Command Template"))
				{
					ImGui::SetClipboardText("$env:MYENGINE_TEXT_TO_3D_COMMAND = \"powershell -ExecutionPolicy Bypass -File tools/ai/hunyuan3d_command_template.ps1 -Prompt \\\"{prompt}\\\" -OutputDir \\\"{output_dir}\\\" -OutputName \\\"{output_name}\\\" -Backend \\\"{backend}\\\" -TimeoutMs {timeout_ms}\"");
					statusMessage = "Copied MYENGINE_TEXT_TO_3D_COMMAND template to clipboard.";
					lastGenerationSucceeded = true;
				}
			}
			if (activeProviderId == "local_text_to_3d_glb_v1" && !hasTextTo3dCommandEnv && !hasHunyuanRepoEnv && !hasHunyuanInferTemplateEnv)
			{
				if (InspectorActionButton("Copy Backend Env Setup"))
				{
					ImGui::SetClipboardText("# Option 1: Point to local Hunyuan3D repo\nsetx HUNYUAN3D_REPO \"C:\\\\path\\\\to\\\\Hunyuan3D\"\n\n# Option 2: Provide explicit infer command template\nsetx HUNYUAN3D_INFER_COMMAND_TEMPLATE \"{python} C:\\\\path\\\\to\\\\infer.py --prompt {prompt_q} --output_glb {output_glb_q}\"");
					statusMessage = "Copied backend env setup template to clipboard.";
					lastGenerationSucceeded = true;
				}
				if (InspectorActionButton("Copy Backend Bridge Self-Check Command"))
				{
					ImGui::SetClipboardText("python tools/hunyuan3d/infer_text_to_3d.py --self_check");
					statusMessage = "Copied backend bridge self-check command to clipboard.";
					lastGenerationSucceeded = true;
				}
				ImGui::TextDisabled("After setting env vars, restart Visual Studio and rerun Test Backend Bridge.");
			}
		}

		ImGui::BeginDisabled(!canGenerate);
		if (InspectorActionButton("Generate AI Asset"))
		{
			MyEngine::AI::AiAssetGenerationRequest request;
			request.prompt = promptBuffer;
			request.outputName = outputNameBuffer;
			request.outputRootDirectory = "assets/generated/ai";
			request.options.generateCollisionMesh = generateCollision;
			request.options.lodCount = lodCount;
			request.options.textureResolution = textureResolution;
			request.options.comfyApiBaseUrl = comfyEndpointBuffer;
			request.options.comfyWorkflowTemplatePath = comfyWorkflowPathBuffer;
			request.options.comfyPromptToken = comfyPromptTokenBuffer;
			request.options.comfyPollIntervalMs = comfyPollIntervalMs;
			request.options.comfyPollTimeoutMs = comfyPollTimeoutMs;
			request.options.textTo3dPythonExe = textTo3dPythonExeBuffer;
			request.options.textTo3dRunnerScriptPath = textTo3dRunnerPathBuffer;
			request.options.textTo3dBackend = textTo3dBackendBuffer;
			request.options.textTo3dRequireGlbOutput = textTo3dRequireGlb;
			request.options.textTo3dTimeoutMs = textTo3dTimeoutMs;
			request.options.textTo3dRetryCount = textTo3dRetryCount;
			request.options.textTo3dRetryBackoffMs = textTo3dRetryBackoffMs;

			MyEngine::AI::AiAssetGenerationResult result = generationService.Generate(request);
			lastGenerationSucceeded = result.success;
			statusMessage = result.message;

			if (result.telemetryDurationMs >= 0)
			{
				backendDurationHistoryMs.push_back(result.telemetryDurationMs);
				if (backendDurationHistoryMs.size() > 32)
					backendDurationHistoryMs.erase(backendDurationHistoryMs.begin());
			}
			if (result.telemetryAttemptCount > 0)
			{
				backendAttemptHistory.push_back(result.telemetryAttemptCount);
				if (backendAttemptHistory.size() > 32)
					backendAttemptHistory.erase(backendAttemptHistory.begin());
			}

			if (result.success)
			{
				auto& pipeline = MyEngine::AssetPipeline::Get();
				for (const auto& filePath : result.generatedFiles)
				{
					if (filePath.empty())
						continue;
					pipeline.QueueImport(filePath);
				}

				lastGeneratedAssetDirectory = result.assetDirectory;
				lastGeneratedPrimaryModelPath = result.primaryModelPath;
				if (!result.assetDirectory.empty())
				{
					ui.assetBrowserPath = result.assetDirectory;
					ui.showAssetBrowser = true;
				}

				statusMessage = "Generated and queued imports from: " + result.assetDirectory;
			}
		}
		ImGui::EndDisabled();

		ImGui::Spacing();
		if (!statusMessage.empty())
		{
			ImVec4 color = lastGenerationSucceeded ? ImVec4(0.30f, 1.00f, 0.50f, 1.0f) : ImVec4(1.00f, 0.45f, 0.35f, 1.0f);
			ImGui::TextColored(color, "%s", statusMessage.c_str());
		}

		if (InspectorActionButton("Open Backend Health Summary JSON"))
		{
			const std::filesystem::path healthPath = ResolvePathFromCurrentOrParents("tools/hunyuan3d/backend_health_summary.json");
			if (!OpenPathWithShell(healthPath.generic_string()))
				statusMessage = "Failed to open backend health summary JSON.";
		}

		if (!lastGeneratedAssetDirectory.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("Last Output Folder: %s", lastGeneratedAssetDirectory.c_str());
			if (InspectorActionButton("Open Last Output Folder"))
			{
				if (!OpenPathWithShell(lastGeneratedAssetDirectory))
					statusMessage = "Failed to open output folder. Open it manually from Asset Browser.";
			}
			if (!lastGeneratedPrimaryModelPath.empty() && InspectorActionButton("Copy Last Model Path"))
			{
				ImGui::SetClipboardText(lastGeneratedPrimaryModelPath.c_str());
				statusMessage = "Copied last generated model path to clipboard.";
				lastGenerationSucceeded = true;
			}
		}

		ImGui::TextDisabled("Tip: after generation, open Asset Browser to inspect imported models/materials.");
		ImGui::End();
	}
}
#endif
