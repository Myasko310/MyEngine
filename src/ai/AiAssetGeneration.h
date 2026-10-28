#pragma once

#include <memory>
#include <string>
#include <vector>

namespace MyEngine::AI
{
	struct AiAssetGenerationOptions
	{
		bool generateCollisionMesh = true;
		int lodCount = 3;
		int textureResolution = 512;
		std::string comfyApiBaseUrl = "http://127.0.0.1:8188";
		std::string comfyWorkflowTemplatePath = "assets/ai/comfyui_workflow_api.json";
		std::string comfyPromptToken = "{{PROMPT}}";
		int comfyPollIntervalMs = 1000;
		int comfyPollTimeoutMs = 120000;
		std::string textTo3dPythonExe = "python";
		std::string textTo3dRunnerScriptPath = "tools/ai/generate_text_to_3d.py";
		std::string textTo3dBackend = "hunyuan3d";
		bool textTo3dRequireGlbOutput = true;
		int textTo3dTimeoutMs = 300000;
		int textTo3dRetryCount = 1;
		int textTo3dRetryBackoffMs = 250;
	};

	struct AiAssetGenerationRequest
	{
		std::string prompt;
		std::string outputName;
		std::string outputRootDirectory = "assets/generated/ai";
		AiAssetGenerationOptions options;
	};

	struct AiAssetGenerationResult
	{
		bool success = false;
		std::string providerId;
		std::string message;
		std::string assetDirectory;
		std::string primaryModelPath;
		std::vector<std::string> lodModelPaths;
		std::string collisionModelPath;
		std::string albedoTexturePath;
		std::string materialPath;
		std::string manifestPath;
		int telemetryDurationMs = -1;
		int telemetryAttemptCount = 0;
		std::vector<std::string> generatedFiles;
	};

	class IAiAssetProvider
	{
	public:
		virtual ~IAiAssetProvider() = default;
		virtual const char* GetProviderId() const = 0;
		virtual bool IsAvailable() const = 0;
		virtual AiAssetGenerationResult Generate(const AiAssetGenerationRequest& request) = 0;
	};

	class AiAssetGenerationService
	{
	public:
		static AiAssetGenerationService& Get();

		void RegisterProvider(const std::shared_ptr<IAiAssetProvider>& provider);
		std::vector<std::string> GetProviderIds() const;
		bool SetActiveProvider(const std::string& providerId);
		const std::string& GetActiveProviderId() const;
		AiAssetGenerationResult Generate(const AiAssetGenerationRequest& request) const;

	private:
		AiAssetGenerationService();

		std::vector<std::shared_ptr<IAiAssetProvider>> m_Providers;
		std::string m_ActiveProviderId;
	};

	std::shared_ptr<IAiAssetProvider> CreateLocalAiAssetProvider();
	std::shared_ptr<IAiAssetProvider> CreateComfyUiLocalAiAssetProvider();
	std::shared_ptr<IAiAssetProvider> CreateLocalTextTo3dGlbAiAssetProvider();
}
