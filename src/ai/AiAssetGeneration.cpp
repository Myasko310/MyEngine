#include "ai/AiAssetGeneration.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

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

			for (int i = 0; i < static_cast<int>(positions.size()); ++i)
			{
				const int index = i + 1;
				obj << "f " << index << "/" << index << "/" << index;
				if (index % 3 == 0)
					obj << "\n";
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
				AiAssetGenerationResult result;
				result.providerId = GetProviderId();

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
		};
	}

	AiAssetGenerationService& AiAssetGenerationService::Get()
	{
		static AiAssetGenerationService service;
		return service;
	}

	AiAssetGenerationService::AiAssetGenerationService()
	{
		auto localProvider = CreateLocalAiAssetProvider();
		if (localProvider)
			RegisterProvider(localProvider);

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
}
