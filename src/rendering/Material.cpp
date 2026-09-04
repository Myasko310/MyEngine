#include "rendering/Material.h"

#include "core/AssetManager.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"

#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace
{
	void WriteVec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const glm::vec3& value)
	{
		writer.StartArray();
		writer.Double(value.x);
		writer.Double(value.y);
		writer.Double(value.z);
		writer.EndArray();
	}

	glm::vec3 ReadVec3(const rapidjson::Value& value, const glm::vec3& fallback)
	{
		if (!value.IsArray() || value.Size() != 3)
			return fallback;

		return glm::vec3(
			value[0].GetFloat(),
			value[1].GetFloat(),
			value[2].GetFloat()
		);
	}

	void WriteTexturePath(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const std::shared_ptr<MyEngine::Texture>& texture)
	{
		if (!texture)
			return;

		writer.Key(key);
		writer.String(texture->GetPath().c_str());
	}
}

namespace MyEngine
{
	bool Material::SaveToFile(const std::string& path) const
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();

		writer.Key("useBaseMaterial");
		writer.Bool(useBaseMaterial);
		writer.Key("baseMaterialPath");
		writer.String(baseMaterialPath.c_str());
		writer.Key("overrideShader");
		writer.Bool(overrideShader);
		writer.Key("overrideSurface");
		writer.Bool(overrideSurface);
		writer.Key("overrideTextures");
		writer.Bool(overrideTextures);
		writer.Key("overridePBR");
		writer.Bool(overridePBR);
		writer.Key("overrideRenderFlags");
		writer.Bool(overrideRenderFlags);

		writer.Key("shaderVertexPath");
		writer.String(shaderVertexPath.c_str());
		writer.Key("shaderFragmentPath");
		writer.String(shaderFragmentPath.c_str());

		writer.Key("albedo");
		WriteVec3(writer, albedo);
		writer.Key("shininess");
		writer.Double(shininess);
		writer.Key("useTexture");
		writer.Bool(useTexture);
		writer.Key("usePBR");
		writer.Bool(usePBR);
		writer.Key("metallic");
		writer.Double(metallic);
		writer.Key("roughness");
		writer.Double(roughness);
		writer.Key("aoStrength");
		writer.Double(aoStrength);
		writer.Key("emissive");
		WriteVec3(writer, emissive);

		WriteTexturePath(writer, "texturePath", texture);
		WriteTexturePath(writer, "albedoMapPath", albedoMap);
		WriteTexturePath(writer, "normalMapPath", normalMap);
		WriteTexturePath(writer, "metallicRoughnessMapPath", metallicRoughnessMap);
		WriteTexturePath(writer, "aoMapPath", aoMap);
		WriteTexturePath(writer, "emissiveMapPath", emissiveMap);

		writer.Key("blendMode");    writer.Int(static_cast<int>(blendMode));
		writer.Key("cullMode");     writer.Int(static_cast<int>(cullMode));
		writer.Key("depthWrite");   writer.Bool(depthWrite);
		writer.Key("depthTest");    writer.Bool(depthTest);
		writer.Key("renderQueue");  writer.Int(renderQueue);

		writer.EndObject();

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;

		out << buffer.GetString();
		return static_cast<bool>(out);
	}

	bool Material::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return false;

		std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		rapidjson::Document doc;
		doc.Parse(json.c_str());
		if (doc.HasParseError() || !doc.IsObject())
			return false;

		m_Path = path;

		if (doc.HasMember("useBaseMaterial") && doc["useBaseMaterial"].IsBool())
			useBaseMaterial = doc["useBaseMaterial"].GetBool();
		if (doc.HasMember("baseMaterialPath") && doc["baseMaterialPath"].IsString())
			baseMaterialPath = doc["baseMaterialPath"].GetString();
		if (doc.HasMember("overrideShader") && doc["overrideShader"].IsBool())
			overrideShader = doc["overrideShader"].GetBool();
		if (doc.HasMember("overrideSurface") && doc["overrideSurface"].IsBool())
			overrideSurface = doc["overrideSurface"].GetBool();
		if (doc.HasMember("overrideTextures") && doc["overrideTextures"].IsBool())
			overrideTextures = doc["overrideTextures"].GetBool();
		if (doc.HasMember("overridePBR") && doc["overridePBR"].IsBool())
			overridePBR = doc["overridePBR"].GetBool();
		if (doc.HasMember("overrideRenderFlags") && doc["overrideRenderFlags"].IsBool())
			overrideRenderFlags = doc["overrideRenderFlags"].GetBool();

		if (doc.HasMember("shaderVertexPath") && doc["shaderVertexPath"].IsString())
			shaderVertexPath = doc["shaderVertexPath"].GetString();
		if (doc.HasMember("shaderFragmentPath") && doc["shaderFragmentPath"].IsString())
			shaderFragmentPath = doc["shaderFragmentPath"].GetString();

		if (!shaderVertexPath.empty() && !shaderFragmentPath.empty())
			shader = AssetManager::LoadShader(shaderVertexPath, shaderFragmentPath);
		if (useBaseMaterial && !baseMaterialPath.empty())
			baseMaterial = AssetManager::LoadMaterial(baseMaterialPath);

		if (doc.HasMember("albedo"))
			albedo = ReadVec3(doc["albedo"], albedo);
		if (doc.HasMember("shininess") && doc["shininess"].IsNumber())
			shininess = doc["shininess"].GetFloat();
		if (doc.HasMember("useTexture") && doc["useTexture"].IsBool())
			useTexture = doc["useTexture"].GetBool();
		if (doc.HasMember("usePBR") && doc["usePBR"].IsBool())
			usePBR = doc["usePBR"].GetBool();
		if (doc.HasMember("metallic") && doc["metallic"].IsNumber())
			metallic = doc["metallic"].GetFloat();
		if (doc.HasMember("roughness") && doc["roughness"].IsNumber())
			roughness = doc["roughness"].GetFloat();
		if (doc.HasMember("aoStrength") && doc["aoStrength"].IsNumber())
			aoStrength = doc["aoStrength"].GetFloat();
		if (doc.HasMember("emissive"))
			emissive = ReadVec3(doc["emissive"], emissive);

		auto loadTextureIfPresent = [&](const char* key, std::shared_ptr<Texture>& target)
		{
			if (doc.HasMember(key) && doc[key].IsString())
				target = AssetManager::LoadTexture(doc[key].GetString());
		};

		loadTextureIfPresent("texturePath", texture);
		loadTextureIfPresent("albedoMapPath", albedoMap);
		loadTextureIfPresent("normalMapPath", normalMap);
		loadTextureIfPresent("metallicRoughnessMapPath", metallicRoughnessMap);
		loadTextureIfPresent("aoMapPath", aoMap);
		loadTextureIfPresent("emissiveMapPath", emissiveMap);

		if (doc.HasMember("blendMode")   && doc["blendMode"].IsInt())
			blendMode   = static_cast<BlendMode>(doc["blendMode"].GetInt());
		if (doc.HasMember("cullMode")    && doc["cullMode"].IsInt())
			cullMode    = static_cast<CullMode>(doc["cullMode"].GetInt());
		if (doc.HasMember("depthWrite")  && doc["depthWrite"].IsBool())
			depthWrite  = doc["depthWrite"].GetBool();
		if (doc.HasMember("depthTest")   && doc["depthTest"].IsBool())
			depthTest   = doc["depthTest"].GetBool();
		if (doc.HasMember("renderQueue") && doc["renderQueue"].IsInt())
			renderQueue = doc["renderQueue"].GetInt();

		return true;
	}

	const Material* Material::GetBaseMaterialResolved() const
	{
		if (!useBaseMaterial || !baseMaterial)
			return nullptr;
		if (baseMaterial.get() == this)
			return nullptr;
		return baseMaterial.get();
	}

	std::shared_ptr<Shader> Material::GetResolvedShader() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideShader || !shader) && base)
			return base->GetResolvedShader();
		return shader;
	}

	glm::vec3 Material::GetResolvedAlbedo() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideSurface && base)
			return base->GetResolvedAlbedo();
		return albedo;
	}

	float Material::GetResolvedShininess() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideSurface && base)
			return base->GetResolvedShininess();
		return shininess;
	}

	std::shared_ptr<Texture> Material::GetResolvedTexture() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !texture) && base)
			return base->GetResolvedTexture();
		return texture;
	}

	bool Material::GetResolvedUseTexture() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideTextures && base)
			return base->GetResolvedUseTexture();
		return useTexture;
	}

	bool Material::GetResolvedUsePBR() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overridePBR && base)
			return base->GetResolvedUsePBR();
		return usePBR;
	}

	float Material::GetResolvedMetallic() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overridePBR && base)
			return base->GetResolvedMetallic();
		return metallic;
	}

	float Material::GetResolvedRoughness() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overridePBR && base)
			return base->GetResolvedRoughness();
		return roughness;
	}

	float Material::GetResolvedAOStrength() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overridePBR && base)
			return base->GetResolvedAOStrength();
		return aoStrength;
	}

	glm::vec3 Material::GetResolvedEmissive() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overridePBR && base)
			return base->GetResolvedEmissive();
		return emissive;
	}

	std::shared_ptr<Texture> Material::GetResolvedAlbedoMap() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !albedoMap) && base)
			return base->GetResolvedAlbedoMap();
		return albedoMap;
	}

	std::shared_ptr<Texture> Material::GetResolvedNormalMap() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !normalMap) && base)
			return base->GetResolvedNormalMap();
		return normalMap;
	}

	std::shared_ptr<Texture> Material::GetResolvedMetallicRoughnessMap() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !metallicRoughnessMap) && base)
			return base->GetResolvedMetallicRoughnessMap();
		return metallicRoughnessMap;
	}

	std::shared_ptr<Texture> Material::GetResolvedAOMap() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !aoMap) && base)
			return base->GetResolvedAOMap();
		return aoMap;
	}

	std::shared_ptr<Texture> Material::GetResolvedEmissiveMap() const
	{
		const Material* base = GetBaseMaterialResolved();
		if ((!overrideTextures || !emissiveMap) && base)
			return base->GetResolvedEmissiveMap();
		return emissiveMap;
	}

	BlendMode Material::GetResolvedBlendMode() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideRenderFlags && base)
			return base->GetResolvedBlendMode();
		return blendMode;
	}

	CullMode Material::GetResolvedCullMode() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideRenderFlags && base)
			return base->GetResolvedCullMode();
		return cullMode;
	}

	bool Material::GetResolvedDepthWrite() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideRenderFlags && base)
			return base->GetResolvedDepthWrite();
		return depthWrite;
	}

	bool Material::GetResolvedDepthTest() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideRenderFlags && base)
			return base->GetResolvedDepthTest();
		return depthTest;
	}

	int Material::GetResolvedRenderQueue() const
	{
		const Material* base = GetBaseMaterialResolved();
		if (!overrideRenderFlags && base)
			return base->GetResolvedRenderQueue();
		return renderQueue;
	}
}
