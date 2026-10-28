#include "ai/AiAssetGeneration.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <rapidjson/document.h>
#include <stb_image_write.h>

namespace MyEngine::AI
{
	namespace
	{
		struct Vec3
		{
			float x;
			float y;
			float z;
		};

		struct Vec2
		{
			float u;
			float v;
		};

		struct LocalShapeParameters
		{
			float width = 1.0f;
			float height = 1.0f;
			float depth = 1.0f;
			float topScale = 1.0f;
		};

		class NullAiAssetProvider final : public IAiAssetProvider
		{
		public:
			const char* GetProviderId() const override
			{
				return "none";
			}

			bool IsAvailable() const override
			{
				return true;
			}

			AiAssetGenerationResult Generate(const AiAssetGenerationRequest&) override
			{
				AiAssetGenerationResult result;
				result.success = false;
				result.providerId = GetProviderId();
				result.message = "AI asset provider is not configured.";
				return result;
			}
		};

		std::string SanitizeToken(const std::string& source)
		{
			std::string token;
			token.reserve(source.size());

			for (unsigned char c : source)
			{
				if (std::isalnum(c))
					token.push_back(static_cast<char>(std::tolower(c)));
				else if (c == ' ' || c == '-' || c == '_')
					token.push_back('_');
			}

			while (!token.empty() && token.front() == '_')
				token.erase(token.begin());
			while (!token.empty() && token.back() == '_')
				token.pop_back();

			if (token.empty())
				return "generated_prop";

			if (token.size() > 48)
				token.resize(48);

			return token;
		}

		std::uint32_t HashPrompt(const std::string& prompt)
		{
			std::uint32_t hash = 2166136261u;
			for (unsigned char c : prompt)
			{
				hash ^= static_cast<std::uint32_t>(c);
				hash *= 16777619u;
			}
			return hash;
		}

		bool WriteTextFile(const std::filesystem::path& filePath, const std::string& text)
		{
			std::ofstream ofs(filePath, std::ios::trunc);
			if (!ofs.is_open())
				return false;

			ofs << text;
			return ofs.good();
		}

		std::string EscapeJsonString(const std::string& value)
		{
			std::string escaped;
			escaped.reserve(value.size());
			for (char c : value)
			{
				switch (c)
				{
				case '\\': escaped += "\\\\"; break;
				case '"': escaped += "\\\""; break;
				case '\n': escaped += "\\n"; break;
				case '\r': escaped += "\\r"; break;
				case '\t': escaped += "\\t"; break;
				default: escaped.push_back(c); break;
				}
			}
			return escaped;
		}

		LocalShapeParameters BuildShapeParameters(std::uint32_t hash)
		{
			const float a = static_cast<float>((hash & 0xFFu)) / 255.0f;
			const float b = static_cast<float>((hash >> 8u) & 0xFFu) / 255.0f;
			const float c = static_cast<float>((hash >> 16u) & 0xFFu) / 255.0f;

			LocalShapeParameters params;
			params.width = 0.6f + a * 1.4f;
			params.height = 0.7f + b * 2.2f;
			params.depth = 0.6f + c * 1.4f;
			params.topScale = 0.65f + (1.0f - a) * 0.35f;
			return params;
		}

		void AppendQuad(std::vector<Vec3>& outPositions, std::vector<Vec3>& outNormals, std::vector<Vec2>& outUvs,
			const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& normal)
		{
			outPositions.push_back(a); outNormals.push_back(normal); outUvs.push_back({ 0.0f, 0.0f });
			outPositions.push_back(b); outNormals.push_back(normal); outUvs.push_back({ 1.0f, 0.0f });
			outPositions.push_back(c); outNormals.push_back(normal); outUvs.push_back({ 1.0f, 1.0f });

			outPositions.push_back(a); outNormals.push_back(normal); outUvs.push_back({ 0.0f, 0.0f });
			outPositions.push_back(c); outNormals.push_back(normal); outUvs.push_back({ 1.0f, 1.0f });
			outPositions.push_back(d); outNormals.push_back(normal); outUvs.push_back({ 0.0f, 1.0f });
		}

		bool WriteTaperedBoxObj(const std::filesystem::path& objPath,
			const LocalShapeParameters& shape,
			const std::string& mtlFileName,
			const std::string& materialName)
		{
			const float hwBottom = shape.width * 0.5f;
			const float hdBottom = shape.depth * 0.5f;
			const float hwTop = hwBottom * shape.topScale;
			const float hdTop = hdBottom * shape.topScale;
			const float h = shape.height;

			const Vec3 b0{ -hwBottom, 0.0f, -hdBottom };
			const Vec3 b1{ hwBottom, 0.0f, -hdBottom };
			const Vec3 b2{ hwBottom, 0.0f, hdBottom };
			const Vec3 b3{ -hwBottom, 0.0f, hdBottom };

			const Vec3 t0{ -hwTop, h, -hdTop };
			const Vec3 t1{ hwTop, h, -hdTop };
			const Vec3 t2{ hwTop, h, hdTop };
			const Vec3 t3{ -hwTop, h, hdTop };

			std::vector<Vec3> positions;
			std::vector<Vec3> normals;
			std::vector<Vec2> uvs;
			positions.reserve(36);
			normals.reserve(36);
			uvs.reserve(36);

			AppendQuad(positions, normals, uvs, b0, b1, t1, t0, { 0.0f, 0.0f, -1.0f });
			AppendQuad(positions, normals, uvs, b1, b2, t2, t1, { 1.0f, 0.0f, 0.0f });
			AppendQuad(positions, normals, uvs, b2, b3, t3, t2, { 0.0f, 0.0f, 1.0f });
			AppendQuad(positions, normals, uvs, b3, b0, t0, t3, { -1.0f, 0.0f, 0.0f });
			AppendQuad(positions, normals, uvs, t0, t1, t2, t3, { 0.0f, 1.0f, 0.0f });
			AppendQuad(positions, normals, uvs, b3, b2, b1, b0, { 0.0f, -1.0f, 0.0f });

			std::ostringstream obj;
			obj << "mtllib " << mtlFileName << "\n";
			obj << "o generated_prop\n";
			obj << "usemtl " << materialName << "\n";

			for (const Vec3& p : positions)
				obj << "v " << p.x << " " << p.y << " " << p.z << "\n";
			for (const Vec2& uv : uvs)
				obj << "vt " << uv.u << " " << uv.v << "\n";
			for (const Vec3& n : normals)
				obj << "vn " << n.x << " " << n.y << " " << n.z << "\n";

			for (int i = 0; i + 2 < static_cast<int>(positions.size()); i += 3)
			{
				const int i0 = i + 1;
				const int i1 = i + 2;
				const int i2 = i + 3;
				obj << "f "
					<< i0 << "/" << i0 << "/" << i0 << " "
					<< i1 << "/" << i1 << "/" << i1 << " "
					<< i2 << "/" << i2 << "/" << i2 << "\n";
			}

			return WriteTextFile(objPath, obj.str());
		}

		bool WriteCollisionBoxObj(const std::filesystem::path& objPath, const LocalShapeParameters& shape)
		{
			const float hw = shape.width * 0.5f;
			const float hd = shape.depth * 0.5f;
			const float h = shape.height;

			std::ostringstream obj;
			obj << "o collision_mesh\n";
			obj << "v " << -hw << " 0 " << -hd << "\n";
			obj << "v " << hw << " 0 " << -hd << "\n";
			obj << "v " << hw << " 0 " << hd << "\n";
			obj << "v " << -hw << " 0 " << hd << "\n";
			obj << "v " << -hw << " " << h << " " << -hd << "\n";
			obj << "v " << hw << " " << h << " " << -hd << "\n";
			obj << "v " << hw << " " << h << " " << hd << "\n";
			obj << "v " << -hw << " " << h << " " << hd << "\n";
			obj << "f 1 2 3\n";
			obj << "f 1 3 4\n";
			obj << "f 5 8 7\n";
			obj << "f 5 7 6\n";
			obj << "f 1 5 6\n";
			obj << "f 1 6 2\n";
			obj << "f 2 6 7\n";
			obj << "f 2 7 3\n";
			obj << "f 3 7 8\n";
			obj << "f 3 8 4\n";
			obj << "f 4 8 5\n";
			obj << "f 4 5 1\n";
			return WriteTextFile(objPath, obj.str());
		}

		bool WriteMaterialFile(const std::filesystem::path& mtlPath, const std::string& materialName, const std::string& textureName)
		{
			std::ostringstream mtl;
			mtl << "newmtl " << materialName << "\n";
			mtl << "Kd 1.000000 1.000000 1.000000\n";
			mtl << "Ka 0.050000 0.050000 0.050000\n";
			mtl << "Ks 0.100000 0.100000 0.100000\n";
			mtl << "Ns 16.000000\n";
			mtl << "map_Kd " << textureName << "\n";
			return WriteTextFile(mtlPath, mtl.str());
		}

		bool WriteTexturePng(const std::filesystem::path& texturePath, int textureResolution, std::uint32_t hash)
		{
			const int res = std::clamp(textureResolution, 128, 2048);
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(res) * static_cast<std::size_t>(res) * 3u);

			const std::uint8_t seedR = static_cast<std::uint8_t>((hash)&0xFFu);
			const std::uint8_t seedG = static_cast<std::uint8_t>((hash >> 8u) & 0xFFu);
			const std::uint8_t seedB = static_cast<std::uint8_t>((hash >> 16u) & 0xFFu);

			for (int y = 0; y < res; ++y)
			{
				for (int x = 0; x < res; ++x)
				{
					const bool checker = ((x / 32) + (y / 32)) % 2 == 0;
					const float u = static_cast<float>(x) / static_cast<float>(res - 1);
					const float v = static_cast<float>(y) / static_cast<float>(res - 1);
					const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(res) + static_cast<std::size_t>(x)) * 3u;

					pixels[idx + 0] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(seedR * (0.55f + 0.45f * u) + (checker ? 18 : -18)), 0, 255));
					pixels[idx + 1] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(seedG * (0.55f + 0.45f * v) + (checker ? 10 : -10)), 0, 255));
					pixels[idx + 2] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(seedB * 0.70f + (checker ? 22 : -22)), 0, 255));
				}
			}

			const int ok = stbi_write_png(texturePath.generic_string().c_str(), res, res, 3, pixels.data(), res * 3);
			return ok != 0;
		}

		bool RunCommandCapture(const std::string& command, std::string& output, int* outExitCode = nullptr)
		{
			output.clear();
			std::array<char, 512> buffer{};
			FILE* pipe = _popen(command.c_str(), "r");
			if (!pipe)
				return false;

			while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
				output += buffer.data();

			const int returnCode = _pclose(pipe);
			if (outExitCode)
				*outExitCode = returnCode;
			return true;
		}

		std::string QuoteArg(const std::string& value)
		{
			std::string escaped;
			escaped.reserve(value.size());
			for (char c : value)
			{
				if (c == '"')
					escaped += "\"\"";
				else
					escaped.push_back(c);
			}
			return "\"" + escaped + "\"";
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

		AiAssetGenerationResult GenerateLocalAssetPackage(const AiAssetGenerationRequest& request, const std::string& providerId)
		{
			AiAssetGenerationResult result;
			result.providerId = providerId;

			const std::string promptToken = SanitizeToken(request.outputName.empty() ? request.prompt : request.outputName);
			if (request.prompt.empty())
			{
				result.message = "Prompt is required.";
				return result;
			}

			const std::filesystem::path outputRoot = request.outputRootDirectory.empty() ? std::filesystem::path("assets/generated/ai") : std::filesystem::path(request.outputRootDirectory);
			const std::uint32_t promptHash = HashPrompt(request.prompt);
			const std::filesystem::path assetDir = outputRoot / (promptToken + "_" + std::to_string(promptHash & 0xFFFFu));

			std::error_code ec;
			std::filesystem::create_directories(assetDir, ec);
			if (ec)
			{
				result.message = "Failed to create output folder: " + assetDir.generic_string();
				return result;
			}

			const std::string textureFileName = promptToken + "_albedo.png";
			const std::string materialFileName = promptToken + ".mtl";
			const std::string modelFileName = promptToken + ".obj";
			const std::string collisionFileName = promptToken + "_collision.obj";

			const std::filesystem::path texturePath = assetDir / textureFileName;
			const std::filesystem::path materialPath = assetDir / materialFileName;
			const std::filesystem::path modelPath = assetDir / modelFileName;
			const std::filesystem::path collisionPath = assetDir / collisionFileName;

			const std::string materialName = promptToken + "_mat";
			LocalShapeParameters baseShape = BuildShapeParameters(promptHash);

			if (!WriteTexturePng(texturePath, request.options.textureResolution, promptHash))
			{
				result.message = "Failed to write generated texture PNG.";
				return result;
			}

			if (!WriteMaterialFile(materialPath, materialName, textureFileName))
			{
				result.message = "Failed to write generated material file.";
				return result;
			}

			if (!WriteTaperedBoxObj(modelPath, baseShape, materialFileName, materialName))
			{
				result.message = "Failed to write generated model OBJ.";
				return result;
			}

			const int lodCount = std::clamp(request.options.lodCount, 1, 4);
			for (int lod = 1; lod < lodCount; ++lod)
			{
				LocalShapeParameters lodShape = baseShape;
				lodShape.topScale = std::min(1.0f, lodShape.topScale + 0.1f * static_cast<float>(lod));
				lodShape.width *= 1.0f - static_cast<float>(lod) * 0.05f;
				lodShape.depth *= 1.0f - static_cast<float>(lod) * 0.05f;

				const std::filesystem::path lodPath = assetDir / (promptToken + "_lod" + std::to_string(lod) + ".obj");
				if (!WriteTaperedBoxObj(lodPath, lodShape, materialFileName, materialName))
				{
					result.message = "Failed to write generated LOD OBJ.";
					return result;
				}
				result.lodModelPaths.push_back(lodPath.generic_string());
			}

			if (request.options.generateCollisionMesh)
			{
				if (!WriteCollisionBoxObj(collisionPath, baseShape))
				{
					result.message = "Failed to write generated collision mesh OBJ.";
					return result;
				}
				result.collisionModelPath = collisionPath.generic_string();
			}

			const std::filesystem::path manifestPath = assetDir / "manifest.ai_asset.json";
			std::ostringstream manifest;
			manifest << "{\n";
			manifest << "  \"provider\": \"" << EscapeJsonString(result.providerId) << "\",\n";
			manifest << "  \"prompt\": \"" << EscapeJsonString(request.prompt) << "\",\n";
			manifest << "  \"assetDirectory\": \"" << EscapeJsonString(assetDir.generic_string()) << "\",\n";
			manifest << "  \"primaryModel\": \"" << EscapeJsonString(modelPath.generic_string()) << "\",\n";
			manifest << "  \"material\": \"" << EscapeJsonString(materialPath.generic_string()) << "\",\n";
			manifest << "  \"albedo\": \"" << EscapeJsonString(texturePath.generic_string()) << "\",\n";
			manifest << "  \"collision\": \"" << EscapeJsonString(result.collisionModelPath) << "\",\n";
			manifest << "  \"lods\": [\n";
			for (std::size_t i = 0; i < result.lodModelPaths.size(); ++i)
			{
				manifest << "    \"" << EscapeJsonString(result.lodModelPaths[i]) << "\"";
				manifest << (i + 1 < result.lodModelPaths.size() ? ",\n" : "\n");
			}
			manifest << "  ]\n";
			manifest << "}\n";

			if (!WriteTextFile(manifestPath, manifest.str()))
			{
				result.message = "Failed to write generated manifest file.";
				return result;
			}

			result.success = true;
			result.assetDirectory = assetDir.generic_string();
			result.primaryModelPath = modelPath.generic_string();
			result.albedoTexturePath = texturePath.generic_string();
			result.materialPath = materialPath.generic_string();
			result.manifestPath = manifestPath.generic_string();
			result.generatedFiles.push_back(modelPath.generic_string());
			result.generatedFiles.push_back(materialPath.generic_string());
			result.generatedFiles.push_back(texturePath.generic_string());
			if (!result.collisionModelPath.empty())
				result.generatedFiles.push_back(result.collisionModelPath);
			for (const auto& lodPath : result.lodModelPaths)
				result.generatedFiles.push_back(lodPath);
			result.generatedFiles.push_back(manifestPath.generic_string());
			result.message = "Generated asset package successfully.";
			return result;
		}

		class LocalSynthesisAiAssetProvider final : public IAiAssetProvider
		{
		public:
			const char* GetProviderId() const override
			{
				return "local_synthesis_v1";
			}

			bool IsAvailable() const override
			{
				return true;
			}

			AiAssetGenerationResult Generate(const AiAssetGenerationRequest& request) override
			{
				return GenerateLocalAssetPackage(request, GetProviderId());
			}
		};

		class LocalTextTo3dGlbAiAssetProvider final : public IAiAssetProvider
		{
		public:
			const char* GetProviderId() const override
			{
				return "local_text_to_3d_glb_v1";
			}

			bool IsAvailable() const override
			{
				return true;
			}

			AiAssetGenerationResult Generate(const AiAssetGenerationRequest& request) override
			{
				AiAssetGenerationResult result;
				result.providerId = GetProviderId();

				if (request.prompt.empty())
				{
					result.message = "Prompt is required.";
					return result;
				}

				const std::string promptToken = SanitizeToken(request.outputName.empty() ? request.prompt : request.outputName);
				const std::filesystem::path outputRoot = request.outputRootDirectory.empty() ? std::filesystem::path("assets/generated/ai") : std::filesystem::path(request.outputRootDirectory);
				const std::uint32_t promptHash = HashPrompt(request.prompt);
				const std::filesystem::path assetDir = outputRoot / (promptToken + "_" + std::to_string(promptHash & 0xFFFFu));

				std::error_code ec;
				std::filesystem::create_directories(assetDir, ec);
				if (ec)
				{
					result.message = "Failed to create output folder: " + assetDir.generic_string();
					return result;
				}

				const std::filesystem::path runnerScriptPath = ResolvePathFromCurrentOrParents(request.options.textTo3dRunnerScriptPath);
				if (!std::filesystem::exists(runnerScriptPath, ec))
				{
					result.message = "Text-to-3D runner script not found: " + request.options.textTo3dRunnerScriptPath;
					return result;
				}

				const std::filesystem::path runnerResultPath = assetDir / "text_to_3d_runner_result.json";
				std::string command = "call " + QuoteArg(request.options.textTo3dPythonExe.empty() ? "python" : request.options.textTo3dPythonExe);
				command += " " + QuoteArg(runnerScriptPath.generic_string());
				command += " --prompt " + QuoteArg(request.prompt);
				command += " --output_dir " + QuoteArg(assetDir.generic_string());
				command += " --output_name " + QuoteArg(promptToken);
				command += " --backend " + QuoteArg(request.options.textTo3dBackend.empty() ? "hunyuan3d" : request.options.textTo3dBackend);
				command += " --result_json " + QuoteArg(runnerResultPath.generic_string());
				command += " --timeout_ms " + std::to_string(std::max(1000, request.options.textTo3dTimeoutMs));
				command += request.options.textTo3dRequireGlbOutput ? " --require_glb 1" : " --require_glb 0";
				command += " --retry_count " + std::to_string(std::max(1, request.options.textTo3dRetryCount));
				command += " --retry_backoff_ms " + std::to_string(std::max(0, request.options.textTo3dRetryBackoffMs));

				std::string commandOutput;
				int runnerExitCode = 0;
				if (!RunCommandCapture(command, commandOutput, &runnerExitCode))
				{
					result.message = "Failed to launch text-to-3D runner process. Verify Python executable path.";
					return result;
				}

				std::ifstream resultFile(runnerResultPath);
				if (!resultFile.is_open())
				{
					result.message = "Text-to-3D runner did not produce result JSON: " + runnerResultPath.generic_string();
					if (!commandOutput.empty())
						result.message += " | output: " + commandOutput;
					return result;
				}

				std::string resultJson((std::istreambuf_iterator<char>(resultFile)), std::istreambuf_iterator<char>());
				rapidjson::Document doc;
				doc.Parse(resultJson.c_str());
				if (doc.HasParseError() || !doc.IsObject())
				{
					result.message = "Text-to-3D runner returned invalid JSON contract.";
					return result;
				}

				const bool success = doc.HasMember("success") && doc["success"].IsBool() && doc["success"].GetBool();
				if (doc.HasMember("message") && doc["message"].IsString())
					result.message = doc["message"].GetString();

				std::string commandTemplateSource;
				if (doc.HasMember("command_template_source") && doc["command_template_source"].IsString())
					commandTemplateSource = doc["command_template_source"].GetString();
				const bool usedDefaultTemplate = doc.HasMember("used_default_template") && doc["used_default_template"].IsBool() && doc["used_default_template"].GetBool();
				std::string backendCommand;
				if (doc.HasMember("command") && doc["command"].IsString())
					backendCommand = doc["command"].GetString();
				std::string backendStderr;
				if (doc.HasMember("stderr") && doc["stderr"].IsString())
					backendStderr = doc["stderr"].GetString();
				std::string backendStdout;
				if (doc.HasMember("stdout") && doc["stdout"].IsString())
					backendStdout = doc["stdout"].GetString();
				int attemptCount = 0;
				if (doc.HasMember("attempt_count") && doc["attempt_count"].IsInt())
					attemptCount = doc["attempt_count"].GetInt();
				int totalDurationMs = -1;
				if (doc.HasMember("total_duration_ms") && doc["total_duration_ms"].IsInt())
					totalDurationMs = doc["total_duration_ms"].GetInt();
				result.telemetryDurationMs = totalDurationMs;
				result.telemetryAttemptCount = attemptCount;

				if (!success)
				{
					if (result.message.empty())
						result.message = "Text-to-3D runner reported failure.";
					if (usedDefaultTemplate)
						result.message += " | template_source=default";
					else if (!commandTemplateSource.empty())
						result.message += " | template_source=" + commandTemplateSource;
					if (!backendCommand.empty())
						result.message += " | command=" + backendCommand;
					if (!backendStderr.empty())
						result.message += " | stderr=" + backendStderr;
					else if (!backendStdout.empty())
						result.message += " | stdout=" + backendStdout;
					if (attemptCount > 0)
						result.message += " | attempts=" + std::to_string(attemptCount);
					if (totalDurationMs >= 0)
						result.message += " | duration_ms=" + std::to_string(totalDurationMs);
					if (!commandOutput.empty())
						result.message += " | output: " + commandOutput;
					return result;
				}

				if (runnerExitCode != 0)
				{
					result.message = "Text-to-3D runner exited with non-zero status despite success payload.";
					if (!backendCommand.empty())
						result.message += " | command=" + backendCommand;
					if (!backendStderr.empty())
						result.message += " | stderr=" + backendStderr;
					if (!commandOutput.empty())
						result.message += " | output: " + commandOutput;
					return result;
				}

				if (!doc.HasMember("glb_path") || !doc["glb_path"].IsString())
				{
					result.message = "Text-to-3D runner success response missing glb_path.";
					return result;
				}

				const std::filesystem::path glbPath = doc["glb_path"].GetString();
				if (!std::filesystem::exists(glbPath, ec))
				{
					result.message = "Text-to-3D runner glb_path does not exist: " + glbPath.generic_string();
					return result;
				}

				result.success = true;
				result.assetDirectory = assetDir.generic_string();
				result.primaryModelPath = glbPath.generic_string();
				result.manifestPath = runnerResultPath.generic_string();
				result.generatedFiles.push_back(result.primaryModelPath);
				result.generatedFiles.push_back(result.manifestPath);

				if (doc.HasMember("albedo_texture_path") && doc["albedo_texture_path"].IsString())
				{
					result.albedoTexturePath = doc["albedo_texture_path"].GetString();
					if (!result.albedoTexturePath.empty())
						result.generatedFiles.push_back(result.albedoTexturePath);
				}
				if (doc.HasMember("collision_path") && doc["collision_path"].IsString())
				{
					result.collisionModelPath = doc["collision_path"].GetString();
					if (!result.collisionModelPath.empty())
						result.generatedFiles.push_back(result.collisionModelPath);
				}
				if (doc.HasMember("lod_paths") && doc["lod_paths"].IsArray())
				{
					for (const auto& lodValue : doc["lod_paths"].GetArray())
					{
						if (!lodValue.IsString())
							continue;
						const std::string lodPath = lodValue.GetString();
						result.lodModelPaths.push_back(lodPath);
						result.generatedFiles.push_back(lodPath);
					}
				}
				if (doc.HasMember("generated_files") && doc["generated_files"].IsArray())
				{
					for (const auto& fileValue : doc["generated_files"].GetArray())
					{
						if (!fileValue.IsString())
							continue;
						const std::string filePath = fileValue.GetString();
						if (std::find(result.generatedFiles.begin(), result.generatedFiles.end(), filePath) == result.generatedFiles.end())
							result.generatedFiles.push_back(filePath);
					}
				}

				if (result.message.empty())
					result.message = "Generated GLB asset from local text-to-3D backend.";
				if (usedDefaultTemplate)
					result.message += " (template_source=default)";
				else if (!commandTemplateSource.empty())
					result.message += " (template_source=" + commandTemplateSource + ")";
				if (attemptCount > 0)
					result.message += " (attempts=" + std::to_string(attemptCount) + ")";
				if (totalDurationMs >= 0)
					result.message += " (duration_ms=" + std::to_string(totalDurationMs) + ")";
				return result;
			}
		};

		class ComfyUiLocalAiAssetProvider final : public IAiAssetProvider
		{
		public:
			const char* GetProviderId() const override
			{
				return "comfyui_local_v1";
			}

			bool IsAvailable() const override
			{
				return true;
			}

			AiAssetGenerationResult Generate(const AiAssetGenerationRequest& request) override
			{
				AiAssetGenerationResult result = GenerateLocalAssetPackage(request, GetProviderId());
				if (!result.success)
					return result;

				if (request.options.comfyWorkflowTemplatePath.empty())
				{
					result.success = false;
					result.message = "ComfyUI workflow template path is empty.";
					return result;
				}

				std::ifstream workflowFile(request.options.comfyWorkflowTemplatePath);
				if (!workflowFile.is_open())
				{
					result.success = false;
					result.message = "Failed to open ComfyUI workflow template: " + request.options.comfyWorkflowTemplatePath;
					return result;
				}

				std::string workflowJson((std::istreambuf_iterator<char>(workflowFile)), std::istreambuf_iterator<char>());
				const std::string promptToken = request.options.comfyPromptToken.empty() ? "{{PROMPT}}" : request.options.comfyPromptToken;
				const std::size_t tokenPos = workflowJson.find(promptToken);
				if (tokenPos == std::string::npos)
				{
					result.success = false;
					result.message = "ComfyUI workflow template does not contain prompt token: " + promptToken;
					return result;
				}
				workflowJson.replace(tokenPos, promptToken.size(), EscapeJsonString(request.prompt));

				const std::filesystem::path assetDir = result.assetDirectory;
				const std::filesystem::path promptRequestPath = assetDir / "comfy_prompt_request.json";
				if (!WriteTextFile(promptRequestPath, workflowJson))
				{
					result.success = false;
					result.message = "Failed to write ComfyUI prompt request payload.";
					return result;
				}

				const std::string baseUrl = request.options.comfyApiBaseUrl.empty() ? "http://127.0.0.1:8188" : request.options.comfyApiBaseUrl;
				std::string submitResponse;
				int submitExitCode = 0;
				const std::string submitCommand = "curl -sS -X POST \"" + baseUrl + "/prompt\" -H \"Content-Type: application/json\" --data-binary @" + promptRequestPath.generic_string();
				if (!RunCommandCapture(submitCommand, submitResponse, &submitExitCode) || submitExitCode != 0)
				{
					result.success = false;
					result.message = "Failed to submit prompt to ComfyUI endpoint. Ensure ComfyUI is running locally.";
					return result;
				}

				rapidjson::Document submitDoc;
				submitDoc.Parse(submitResponse.c_str());
				if (submitDoc.HasParseError() || !submitDoc.IsObject() || !submitDoc.HasMember("prompt_id") || !submitDoc["prompt_id"].IsString())
				{
					result.success = false;
					result.message = "ComfyUI returned an invalid /prompt response.";
					return result;
				}

				const std::string promptId = submitDoc["prompt_id"].GetString();
				const int pollIntervalMs = std::clamp(request.options.comfyPollIntervalMs, 100, 10000);
				const int pollTimeoutMs = std::clamp(request.options.comfyPollTimeoutMs, 1000, 600000);
				const int maxAttempts = std::max(1, pollTimeoutMs / pollIntervalMs);

				std::string imageFileName;
				std::string imageSubfolder;
				std::string imageType = "output";
				for (int attempt = 0; attempt < maxAttempts; ++attempt)
				{
					std::string historyResponse;
					int historyExitCode = 0;
					const std::string historyCommand = "curl -sS \"" + baseUrl + "/history/" + promptId + "\"";
					if (!RunCommandCapture(historyCommand, historyResponse, &historyExitCode) || historyExitCode != 0)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
						continue;
					}

					rapidjson::Document historyDoc;
					historyDoc.Parse(historyResponse.c_str());
					if (!historyDoc.IsObject() || !historyDoc.HasMember(promptId.c_str()) || !historyDoc[promptId.c_str()].IsObject())
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
						continue;
					}

					const rapidjson::Value& promptNode = historyDoc[promptId.c_str()];
					if (!promptNode.HasMember("outputs") || !promptNode["outputs"].IsObject())
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
						continue;
					}

					const rapidjson::Value& outputs = promptNode["outputs"];
					for (auto member = outputs.MemberBegin(); member != outputs.MemberEnd(); ++member)
					{
						if (!member->value.IsObject() || !member->value.HasMember("images") || !member->value["images"].IsArray())
							continue;

						const rapidjson::Value& images = member->value["images"];
						if (images.Empty() || !images[0].IsObject())
							continue;

						const rapidjson::Value& img = images[0];
						if (img.HasMember("filename") && img["filename"].IsString())
							imageFileName = img["filename"].GetString();
						if (img.HasMember("subfolder") && img["subfolder"].IsString())
							imageSubfolder = img["subfolder"].GetString();
						if (img.HasMember("type") && img["type"].IsString())
							imageType = img["type"].GetString();
						break;
					}

					if (!imageFileName.empty())
						break;

					std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
				}

				if (imageFileName.empty())
				{
					result.success = false;
					result.message = "ComfyUI generation timed out or produced no images.";
					return result;
				}

				std::string viewUrl = baseUrl + "/view?filename=" + imageFileName + "&type=" + imageType;
				if (!imageSubfolder.empty())
					viewUrl += "&subfolder=" + imageSubfolder;

				const std::string downloadCommand = "curl -sS -L \"" + viewUrl + "\" -o " + result.albedoTexturePath;
				std::string downloadOutput;
				int downloadExitCode = 0;
				if (!RunCommandCapture(downloadCommand, downloadOutput, &downloadExitCode) || downloadExitCode != 0)
				{
					result.success = false;
					result.message = "Failed to download ComfyUI generated texture.";
					return result;
				}

				result.message = "Generated asset package with ComfyUI texture synthesis.";
				return result;
			}
		};
	}

	AiAssetGenerationService& AiAssetGenerationService::Get()
	{
		static AiAssetGenerationService service;
		return service;
	}

	AiAssetGenerationService::AiAssetGenerationService()
	{
		auto textTo3dProvider = CreateLocalTextTo3dGlbAiAssetProvider();
		if (textTo3dProvider)
			RegisterProvider(textTo3dProvider);

		auto localProvider = CreateLocalAiAssetProvider();
		if (localProvider)
			RegisterProvider(localProvider);

		auto comfyProvider = CreateComfyUiLocalAiAssetProvider();
		if (comfyProvider)
			RegisterProvider(comfyProvider);

		if (m_Providers.empty())
			RegisterProvider(std::make_shared<NullAiAssetProvider>());
	}

	void AiAssetGenerationService::RegisterProvider(const std::shared_ptr<IAiAssetProvider>& provider)
	{
		if (!provider)
			return;

		const std::string providerId = provider->GetProviderId() ? provider->GetProviderId() : "";
		if (providerId.empty())
			return;

		auto it = std::find_if(m_Providers.begin(), m_Providers.end(), [&](const std::shared_ptr<IAiAssetProvider>& existing)
		{
			return existing && existing->GetProviderId() && providerId == existing->GetProviderId();
		});

		if (it != m_Providers.end())
		{
			*it = provider;
		}
		else
		{
			m_Providers.push_back(provider);
		}

		if (m_ActiveProviderId.empty())
			m_ActiveProviderId = providerId;
	}

	std::vector<std::string> AiAssetGenerationService::GetProviderIds() const
	{
		std::vector<std::string> providerIds;
		providerIds.reserve(m_Providers.size());

		for (const auto& provider : m_Providers)
		{
			if (provider && provider->GetProviderId())
				providerIds.push_back(provider->GetProviderId());
		}

		return providerIds;
	}

	bool AiAssetGenerationService::SetActiveProvider(const std::string& providerId)
	{
		const auto it = std::find_if(m_Providers.begin(), m_Providers.end(), [&](const std::shared_ptr<IAiAssetProvider>& provider)
		{
			return provider && provider->GetProviderId() && providerId == provider->GetProviderId();
		});

		if (it == m_Providers.end())
			return false;

		m_ActiveProviderId = providerId;
		return true;
	}

	const std::string& AiAssetGenerationService::GetActiveProviderId() const
	{
		return m_ActiveProviderId;
	}

	AiAssetGenerationResult AiAssetGenerationService::Generate(const AiAssetGenerationRequest& request) const
	{
		const auto it = std::find_if(m_Providers.begin(), m_Providers.end(), [&](const std::shared_ptr<IAiAssetProvider>& provider)
		{
			return provider && provider->GetProviderId() && m_ActiveProviderId == provider->GetProviderId();
		});

		if (it == m_Providers.end() || !(*it))
		{
			AiAssetGenerationResult result;
			result.success = false;
			result.message = "No active AI asset provider selected.";
			return result;
		}

		if (!(*it)->IsAvailable())
		{
			AiAssetGenerationResult result;
			result.success = false;
			result.providerId = (*it)->GetProviderId() ? (*it)->GetProviderId() : "";
			result.message = "Selected AI asset provider is unavailable.";
			return result;
		}

		AiAssetGenerationResult result = (*it)->Generate(request);
		if (result.providerId.empty() && (*it)->GetProviderId())
			result.providerId = (*it)->GetProviderId();
		return result;
	}

	std::shared_ptr<IAiAssetProvider> CreateLocalAiAssetProvider()
	{
		return std::make_shared<LocalSynthesisAiAssetProvider>();
	}

	std::shared_ptr<IAiAssetProvider> CreateComfyUiLocalAiAssetProvider()
	{
		return std::make_shared<ComfyUiLocalAiAssetProvider>();
	}

	std::shared_ptr<IAiAssetProvider> CreateLocalTextTo3dGlbAiAssetProvider()
	{
		return std::make_shared<LocalTextTo3dGlbAiAssetProvider>();
	}
}
