#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef MYENGINE_TEST_SOURCE_DIR
#define MYENGINE_TEST_SOURCE_DIR "."
#endif

#include "ai/AiAssetGeneration.h"

int main()
{
	namespace fs = std::filesystem;
	auto& service = MyEngine::AI::AiAssetGenerationService::Get();
	if (!service.SetActiveProvider("local_text_to_3d_glb_v1"))
	{
		std::cerr << "AiBackendContractTests: failed to select local_text_to_3d_glb_v1 provider" << std::endl;
		return 1;
	}

	_putenv("MYENGINE_TEXT_TO_3D_COMMAND=");
	_putenv("HUNYUAN3D_REPO=");
	_putenv("HUNYUAN3D_INFER_COMMAND_TEMPLATE=");

	MyEngine::AI::AiAssetGenerationRequest request;
	request.prompt = "backend contract probe";
	request.outputName = "backend_contract_probe";
	request.outputRootDirectory = (fs::temp_directory_path() / "myengine_ai_backend_contract").generic_string();
	request.options.textTo3dPythonExe = "python";
	request.options.textTo3dRunnerScriptPath = "tools/ai/generate_text_to_3d.py";
	request.options.textTo3dBackend = "hunyuan3d";
	request.options.textTo3dTimeoutMs = 5000;
	request.options.textTo3dRequireGlbOutput = true;

	std::error_code ec;
	fs::remove_all(request.outputRootDirectory, ec);

	const MyEngine::AI::AiAssetGenerationResult result = service.Generate(request);
	if (result.success)
	{
		std::cerr << "AiBackendContractTests: expected failure when backend is unconfigured" << std::endl;
		return 1;
	}

	const bool hasSetupHint =
		result.message.find("No text-to-3D backend command configured") != std::string::npos ||
		result.message.find("Hunyuan3D") != std::string::npos;
	if (!hasSetupHint)
	{
		std::cerr << "AiBackendContractTests: missing setup guidance in failure message: " << result.message << std::endl;
		return 1;
	}

	const bool hasStructuredDiagnostics =
		result.message.find("template_source=") != std::string::npos ||
		result.message.find("command=") != std::string::npos;
	if (!hasStructuredDiagnostics)
	{
		std::cerr << "AiBackendContractTests: missing structured diagnostics in failure message: " << result.message << std::endl;
		return 1;
	}

	const fs::path outputRoot(request.outputRootDirectory);
	bool hasResultManifest = false;
	for (const auto& entry : fs::recursive_directory_iterator(outputRoot, ec))
	{
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		if (entry.path().filename() == "text_to_3d_runner_result.json")
		{
			hasResultManifest = true;
			std::ifstream ifs(entry.path());
			std::string json((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			if (json.find("\"command_template_source\"") == std::string::npos)
			{
				std::cerr << "AiBackendContractTests: runner result JSON missing command_template_source" << std::endl;
				return 1;
			}
			if (json.find("\"total_duration_ms\"") == std::string::npos)
			{
				std::cerr << "AiBackendContractTests: runner result JSON missing total_duration_ms telemetry" << std::endl;
				return 1;
			}
			if (json.find("\"duration_ms\"") == std::string::npos)
			{
				std::cerr << "AiBackendContractTests: runner result JSON missing attempt duration_ms telemetry" << std::endl;
				return 1;
			}
			if (json.find("\"retry_count\"") == std::string::npos || json.find("\"retry_backoff_ms\"") == std::string::npos)
			{
				std::cerr << "AiBackendContractTests: runner result JSON missing retry tuning telemetry" << std::endl;
				return 1;
			}
			break;
		}
	}

	if (!hasResultManifest)
	{
		std::cerr << "AiBackendContractTests: expected text_to_3d_runner_result.json to be emitted" << std::endl;
		return 1;
	}

	const fs::path successRoot = fs::temp_directory_path() / "myengine_ai_backend_contract_success";
	fs::remove_all(successRoot, ec);
	fs::create_directories(successRoot, ec);
	const std::string successCommand =
		"powershell -ExecutionPolicy Bypass -Command \"$outDir='{output_dir}'; "
		"$name='{output_name}'; "
		"$path=Join-Path $outDir ($name + '.glb'); "
		"Set-Content -Path $path -Value 'stub-glb' -NoNewline\"";
	_putenv_s("MYENGINE_TEXT_TO_3D_COMMAND", successCommand.c_str());

	MyEngine::AI::AiAssetGenerationRequest successRequest;
	successRequest.prompt = "backend configured success path";
	successRequest.outputName = "backend_contract_success";
	successRequest.outputRootDirectory = successRoot.generic_string();
	successRequest.options.textTo3dPythonExe = "python";
	successRequest.options.textTo3dRunnerScriptPath = "tools/ai/generate_text_to_3d.py";
	successRequest.options.textTo3dBackend = "hunyuan3d";
	successRequest.options.textTo3dTimeoutMs = 5000;
	successRequest.options.textTo3dRequireGlbOutput = true;

	const MyEngine::AI::AiAssetGenerationResult successResult = service.Generate(successRequest);
	if (!successResult.success)
	{
		std::cerr << "AiBackendContractTests: configured backend success-path test failed: " << successResult.message << std::endl;
		return 1;
	}
	if (successResult.primaryModelPath.empty() || !fs::exists(successResult.primaryModelPath))
	{
		std::cerr << "AiBackendContractTests: configured backend success-path missing GLB output" << std::endl;
		return 1;
	}
	if (successResult.message.find("template_source=") == std::string::npos)
	{
		std::cerr << "AiBackendContractTests: configured backend success-path missing template source metadata" << std::endl;
		return 1;
	}

	const fs::path healthJsonPath = fs::temp_directory_path() / "myengine_ai_backend_health_summary.json";
	fs::remove(healthJsonPath, ec);
	_putenv_s("HUNYUAN3D_INFER_COMMAND_TEMPLATE", "{python} -c \"print('health-check')\"");
	const std::string inferBridgeScriptPath = std::string(MYENGINE_TEST_SOURCE_DIR) + "/tools/hunyuan3d/infer_text_to_3d.py";
	const std::string healthCheckCommand =
		"python \"" + inferBridgeScriptPath + "\" --self_check --health_json \"" + healthJsonPath.generic_string() + "\"";
	const int healthCheckExit = std::system(healthCheckCommand.c_str());
	if (healthCheckExit != 0)
	{
		std::cerr << "AiBackendContractTests: backend health self-check command failed with exit code " << healthCheckExit << std::endl;
		return 1;
	}
	if (!fs::exists(healthJsonPath))
	{
		std::cerr << "AiBackendContractTests: backend health summary JSON was not generated" << std::endl;
		return 1;
	}

	std::ifstream healthIfs(healthJsonPath);
	std::string healthJson((std::istreambuf_iterator<char>(healthIfs)), std::istreambuf_iterator<char>());
	if (healthJson.find("\"success\": true") == std::string::npos)
	{
		std::cerr << "AiBackendContractTests: backend health summary does not report success" << std::endl;
		return 1;
	}
	if (healthJson.find("\"template_count\"") == std::string::npos)
	{
		std::cerr << "AiBackendContractTests: backend health summary missing template_count" << std::endl;
		return 1;
	}
	if (healthJson.find("\"duration_ms\"") == std::string::npos)
	{
		std::cerr << "AiBackendContractTests: backend health summary missing duration_ms telemetry" << std::endl;
		return 1;
	}

	std::cout << "AiBackendContractTests passed" << std::endl;
	return 0;
}
