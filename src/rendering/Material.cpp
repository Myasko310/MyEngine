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

		if (doc.HasMember("shaderVertexPath") && doc["shaderVertexPath"].IsString())
			shaderVertexPath = doc["shaderVertexPath"].GetString();
		if (doc.HasMember("shaderFragmentPath") && doc["shaderFragmentPath"].IsString())
			shaderFragmentPath = doc["shaderFragmentPath"].GetString();

		if (!shaderVertexPath.empty() && !shaderFragmentPath.empty())
			shader = AssetManager::LoadShader(shaderVertexPath, shaderFragmentPath);

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

		return true;
	}
}
