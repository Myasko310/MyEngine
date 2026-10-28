#include "ai/AiAssetGeneration.h"

#include <filesystem>
#include <iostream>

bool AiAssetGenerationSmokeTest()
{
	namespace fs = std::filesystem;

	MyEngine::AI::AiAssetGenerationRequest request;
	request.prompt = "Ancient stone lantern with moss and chipped edges";
	request.outputName = "stone_lantern";
	request.outputRootDirectory = (fs::temp_directory_path() / "myengine_ai_asset_smoke").generic_string();
	request.options.generateCollisionMesh = true;
	request.options.lodCount = 3;
	request.options.textureResolution = 256;

	std::error_code ec;
	fs::remove_all(request.outputRootDirectory, ec);

	auto& service = MyEngine::AI::AiAssetGenerationService::Get();
	MyEngine::AI::AiAssetGenerationResult result = service.Generate(request);
	if (!result.success)
	{
		std::cerr << "AiAssetGenerationSmokeTest: generation failed: " << result.message << std::endl;
		return false;
	}

	if (result.primaryModelPath.empty() || !fs::exists(result.primaryModelPath))
	{
		std::cerr << "AiAssetGenerationSmokeTest: missing primary model output" << std::endl;
		return false;
	}

	if (result.albedoTexturePath.empty() || !fs::exists(result.albedoTexturePath))
	{
		std::cerr << "AiAssetGenerationSmokeTest: missing albedo texture output" << std::endl;
		return false;
	}

	if (result.materialPath.empty() || !fs::exists(result.materialPath))
	{
		std::cerr << "AiAssetGenerationSmokeTest: missing material output" << std::endl;
		return false;
	}

	if (result.manifestPath.empty() || !fs::exists(result.manifestPath))
	{
		std::cerr << "AiAssetGenerationSmokeTest: missing manifest output" << std::endl;
		return false;
	}

	if (result.collisionModelPath.empty() || !fs::exists(result.collisionModelPath))
	{
		std::cerr << "AiAssetGenerationSmokeTest: missing collision mesh output" << std::endl;
		return false;
	}

	if (result.lodModelPaths.size() != 2)
	{
		std::cerr << "AiAssetGenerationSmokeTest: expected 2 generated LOD files for lodCount=3" << std::endl;
		return false;
	}

	for (const auto& lodPath : result.lodModelPaths)
	{
		if (!fs::exists(lodPath))
		{
			std::cerr << "AiAssetGenerationSmokeTest: missing LOD file: " << lodPath << std::endl;
			return false;
		}
	}

	if (result.generatedFiles.size() < 6)
	{
		std::cerr << "AiAssetGenerationSmokeTest: generated file list too small" << std::endl;
		return false;
	}

	std::cout << "[AIAssetGeneration] Local provider output package generation passed" << std::endl;
	return true;
}
