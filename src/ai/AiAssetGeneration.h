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
}
