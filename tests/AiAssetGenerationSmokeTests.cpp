#include "ai/AiAssetGeneration.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

bool AiAssetGenerationSmokeTest()
{
	namespace fs = std::filesystem;
	auto& service = MyEngine::AI::AiAssetGenerationService::Get();
	const std::vector<std::string> providers = service.GetProviderIds();

	const bool hasLocalProvider = std::find(providers.begin(), providers.end(), "local_synthesis_v1") != providers.end();
	const bool hasComfyProvider = std::find(providers.begin(), providers.end(), "comfyui_local_v1") != providers.end();
	const bool hasTextTo3dProvider = std::find(providers.begin(), providers.end(), "local_text_to_3d_glb_v1") != providers.end();
	if (!hasLocalProvider || !hasComfyProvider || !hasTextTo3dProvider)
	{
		std::cerr << "AiAssetGenerationSmokeTest: expected local, ComfyUI, and text-to-3D providers to be registered" << std::endl;
		return false;
	}

	if (!service.SetActiveProvider("local_synthesis_v1"))
	{
		std::cerr << "AiAssetGenerationSmokeTest: failed to select local provider" << std::endl;
		return false;
	}

	MyEngine::AI::AiAssetGenerationRequest request;
	request.prompt = "Ancient stone lantern with moss and chipped edges";
	request.outputName = "stone_lantern";
	request.outputRootDirectory = (fs::temp_directory_path() / "myengine_ai_asset_smoke").generic_string();
	request.options.generateCollisionMesh = true;
	request.options.lodCount = 3;
	request.options.textureResolution = 256;

	std::error_code ec;
	fs::remove_all(request.outputRootDirectory, ec);

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

	if (!service.SetActiveProvider("local_text_to_3d_glb_v1"))
	{
		std::cerr << "AiAssetGenerationSmokeTest: failed to select text-to-3D GLB provider" << std::endl;
		return false;
	}

	_putenv("MYENGINE_TEXT_TO_3D_COMMAND=");
	MyEngine::AI::AiAssetGenerationRequest textTo3dRequest = request;
	textTo3dRequest.outputName = "stone_lantern_text3d";
	MyEngine::AI::AiAssetGenerationResult textTo3dResult = service.Generate(textTo3dRequest);
	if (textTo3dResult.success)
	{
		std::cerr << "AiAssetGenerationSmokeTest: text-to-3D missing-backend test unexpectedly succeeded" << std::endl;
		return false;
	}
	if (textTo3dResult.message.find("No text-to-3D backend command configured") == std::string::npos &&
		textTo3dResult.message.find("runner") == std::string::npos &&
		textTo3dResult.message.find("Default Hunyuan3D template command failed") == std::string::npos &&
		textTo3dResult.message.find("Hunyuan3D") == std::string::npos)
	{
		std::cerr << "AiAssetGenerationSmokeTest: text-to-3D failure path did not return actionable setup guidance" << std::endl;
		return false;
	}
	if (textTo3dResult.message.find("template_source=") == std::string::npos &&
		textTo3dResult.message.find("command=") == std::string::npos)
	{
		std::cerr << "AiAssetGenerationSmokeTest: text-to-3D failure path did not include backend contract diagnostics" << std::endl;
		return false;
	}

	if (!service.SetActiveProvider("comfyui_local_v1"))
	{
		std::cerr << "AiAssetGenerationSmokeTest: failed to select ComfyUI provider" << std::endl;
		return false;
	}

	MyEngine::AI::AiAssetGenerationRequest comfyRequest = request;
	comfyRequest.outputName = "stone_lantern_comfy";
	comfyRequest.options.comfyWorkflowTemplatePath = "assets/ai/nonexistent_workflow_for_smoke.json";
	MyEngine::AI::AiAssetGenerationResult comfyResult = service.Generate(comfyRequest);
	if (comfyResult.success)
	{
		std::cerr << "AiAssetGenerationSmokeTest: ComfyUI failure-path test unexpectedly succeeded" << std::endl;
		return false;
	}

	if (comfyResult.message.find("workflow") == std::string::npos && comfyResult.message.find("ComfyUI") == std::string::npos)
	{
		std::cerr << "AiAssetGenerationSmokeTest: ComfyUI failure path did not return an actionable message" << std::endl;
		return false;
	}

	service.SetActiveProvider("local_synthesis_v1");
	std::cout << "[AIAssetGeneration] Provider registration, local generation, text-to-3D setup guidance/contracts, and ComfyUI failure handling passed" << std::endl;
	return true;
}
