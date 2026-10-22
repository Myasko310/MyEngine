#include "character/CharacterPreset.h"

#include "rendering/Skeleton.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#include <glm/common.hpp>

namespace MyEngine::CharacterPreset
{
	namespace
	{
		constexpr int kFormatVersion = 3;

		void WriteVec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const glm::vec3& value)
		{
			writer.Key(key);
			writer.StartArray();
			writer.Double(value.x);
			writer.Double(value.y);
			writer.Double(value.z);
			writer.EndArray();
		}

		bool ReadVec3(const rapidjson::Value& object, const char* key, glm::vec3& outValue)
		{
			if (!object.HasMember(key) || !object[key].IsArray() || object[key].Size() != 3)
				return false;
			const auto& array = object[key];
			for (rapidjson::SizeType i = 0; i < 3; ++i)
			{
				if (!array[i].IsNumber())
					return false;
				outValue[static_cast<int>(i)] = array[i].GetFloat();
			}
			return true;
		}

		void WriteMaterial(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
			const char* key, const CharacterMaterialOverride& material)
		{
			writer.Key(key);
			writer.StartObject();
			writer.Key("enabled"); writer.Bool(material.enabled);
			writer.Key("materialPath"); writer.String(material.materialPath.c_str());
			writer.Key("albedoTexturePath"); writer.String(material.albedoTexturePath.c_str());
			WriteVec3(writer, "tint", material.tint);
			writer.Key("overrideShininess"); writer.Bool(material.overrideShininess);
			writer.Key("shininess"); writer.Double(material.shininess);
			writer.Key("overridePBR"); writer.Bool(material.overridePBR);
			writer.Key("metallic"); writer.Double(material.metallic);
			writer.Key("roughness"); writer.Double(material.roughness);
			WriteVec3(writer, "emissive", material.emissive);
			writer.EndObject();
		}

		bool ReadMaterial(const rapidjson::Value& parent, const char* key, CharacterMaterialOverride& material)
		{
			material = {};
			if (!parent.HasMember(key))
				return true;
			const auto& object = parent[key];
			if (!object.IsObject())
				return false;
			auto readPath = [&](const char* name, std::string& value)
			{
				if (!object.HasMember(name)) return true;
				if (!object[name].IsString()) return false;
				value.assign(object[name].GetString(), object[name].GetStringLength());
				return value.find('\0') == std::string::npos;
			};
			auto readBool = [&](const char* name, bool& value)
			{
				if (!object.HasMember(name)) return true;
				if (!object[name].IsBool()) return false;
				value = object[name].GetBool();
				return true;
			};
			auto readScalar = [&](const char* name, float& value, float min, float max)
			{
				if (!object.HasMember(name)) return true;
				if (!object[name].IsNumber()) return false;
				const double number = object[name].GetDouble();
				if (!std::isfinite(number) || number < min || number > max) return false;
				value = static_cast<float>(number);
				return true;
			};
			auto readColor = [&](const char* name, glm::vec3& value, float max)
			{
				if (!object.HasMember(name)) return true;
				const auto& array = object[name];
				if (!array.IsArray() || array.Size() != 3) return false;
				for (int i = 0; i < 3; ++i)
				{
					if (!array[i].IsNumber()) return false;
					const double number = array[i].GetDouble();
					if (!std::isfinite(number) || number < 0.0 || number > max) return false;
					value[i] = static_cast<float>(number);
				}
				return true;
			};
			return readBool("enabled", material.enabled)
				&& readPath("materialPath", material.materialPath)
				&& readPath("albedoTexturePath", material.albedoTexturePath)
				&& readBool("overrideShininess", material.overrideShininess)
				&& readBool("overridePBR", material.overridePBR)
				&& readScalar("shininess", material.shininess, 1.0f, 256.0f)
				&& readScalar("metallic", material.metallic, 0.0f, 1.0f)
				&& readScalar("roughness", material.roughness, 0.04f, 1.0f)
				&& readColor("tint", material.tint, 1.0f)
				&& readColor("emissive", material.emissive, 100.0f);
		}
	}

	std::string ToJson(const CharacterCustomizationComponent& component, const Skeleton* skeleton)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("type"); writer.String("CharacterPreset");
		writer.Key("version"); writer.Int(kFormatVersion);
		writer.Key("enabled"); writer.Bool(component.enabled);
		WriteMaterial(writer, "bodyMaterial", component.bodyMaterial);
		if (skeleton)
		{
			writer.Key("skeletonBoneCount");
			writer.Int(static_cast<int>(skeleton->GetBoneCount()));
		}

		// Emit in a stable (sorted) order so diffs stay readable.
		std::map<std::string, const BoneOverride*> sorted;
		for (const auto& [name, value] : component.boneOverrides)
		{
			if (!value.IsIdentity())
				sorted[name] = &value;
		}

		writer.Key("boneOverrides");
		writer.StartArray();
		for (const auto& [name, value] : sorted)
		{
			writer.StartObject();
			writer.Key("bone"); writer.String(name.c_str());
			WriteVec3(writer, "positionOffset", value->positionOffset);
			WriteVec3(writer, "rotationOffsetDegrees", value->rotationOffsetDegrees);
			WriteVec3(writer, "scaleMultiplier", value->scaleMultiplier);
			if (value->counterScaleChildren)
			{
				writer.Key("counterScaleChildren"); writer.Bool(true);
			}
			writer.EndObject();
		}
		writer.EndArray();

		// Sliders: default sliders persist only their value (mappings are
		// rig-defined), skipping ones left at 0. User-edited sliders persist
		// their full definition (v3) so the mapping editor's work round-trips.
		writer.Key("bodySliders");
		writer.StartArray();
		for (const auto& slider : component.bodySliders)
		{
			if (slider.IsDefault() && !slider.customMappings)
				continue;
			writer.StartObject();
			writer.Key("name"); writer.String(slider.name.c_str());
			writer.Key("value"); writer.Double(slider.value);
			if (slider.customMappings)
			{
				writer.Key("custom"); writer.Bool(true);
				writer.Key("min"); writer.Double(slider.minValue);
				writer.Key("max"); writer.Double(slider.maxValue);
				writer.Key("mappings");
				writer.StartArray();
				for (const auto& mapping : slider.mappings)
				{
					writer.StartObject();
					writer.Key("bone"); writer.String(mapping.boneName.c_str());
					WriteVec3(writer, "positionPerUnit", mapping.positionPerUnit);
					WriteVec3(writer, "rotationDegreesPerUnit", mapping.rotationDegreesPerUnit);
					WriteVec3(writer, "scalePerUnit", mapping.scalePerUnit);
					if (mapping.counterScaleChildren)
					{
						writer.Key("counterScaleChildren"); writer.Bool(true);
					}
					writer.EndObject();
				}
				writer.EndArray();
			}
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("sockets");
		writer.StartArray();
		for (const auto& socket : component.sockets)
		{
			writer.StartObject();
			writer.Key("name"); writer.String(socket.name.c_str());
			writer.Key("bone"); writer.String(socket.boneName.c_str());
			writer.Key("mesh"); writer.String(socket.meshPath.c_str());
			WriteVec3(writer, "positionOffset", socket.positionOffset);
			WriteVec3(writer, "rotationOffsetDegrees", socket.rotationOffsetDegrees);
			WriteVec3(writer, "scale", socket.scale);
			writer.Key("visible"); writer.Bool(socket.visible);
			WriteMaterial(writer, "material", socket.material);
			writer.EndObject();
		}
		writer.EndArray();
		writer.EndObject();

		return buffer.GetString();
	}

	bool FromJson(const std::string& json, CharacterCustomizationComponent& outComponent)
	{
		rapidjson::Document doc;
		doc.Parse(json.c_str());
		if (doc.HasParseError() || !doc.IsObject())
			return false;

		if (doc.HasMember("type") && doc["type"].IsString() && std::string(doc["type"].GetString()) != "CharacterPreset")
			return false;

		CharacterMaterialOverride bodyMaterial;
		if (!ReadMaterial(doc, "bodyMaterial", bodyMaterial))
			return false;
		if (doc.HasMember("sockets") && doc["sockets"].IsArray())
		{
			for (const auto& entry : doc["sockets"].GetArray())
			{
				CharacterMaterialOverride material;
				if (entry.IsObject() && !ReadMaterial(entry, "material", material))
					return false;
			}
		}
		outComponent.bodyMaterial = bodyMaterial;
		outComponent.boneOverrides.clear();
		outComponent.sockets.clear();
		for (auto& slider : outComponent.bodySliders)
		{
			slider.value = 0.0f;
			if (slider.customMappings)
			{
				// A loaded preset fully defines its custom sliders; drop stale ones.
				slider.customMappings = false;
				slider.mappings.clear();
			}
		}
		outComponent.enabled = !doc.HasMember("enabled") || !doc["enabled"].IsBool() || doc["enabled"].GetBool();

		if (doc.HasMember("boneOverrides") && doc["boneOverrides"].IsArray())
		{
			for (const auto& entry : doc["boneOverrides"].GetArray())
			{
				if (!entry.IsObject() || !entry.HasMember("bone") || !entry["bone"].IsString())
					continue;

				BoneOverride boneOverride;
				ReadVec3(entry, "positionOffset", boneOverride.positionOffset);
				ReadVec3(entry, "rotationOffsetDegrees", boneOverride.rotationOffsetDegrees);
				ReadVec3(entry, "scaleMultiplier", boneOverride.scaleMultiplier);
				if (entry.HasMember("counterScaleChildren") && entry["counterScaleChildren"].IsBool())
					boneOverride.counterScaleChildren = entry["counterScaleChildren"].GetBool();
				outComponent.boneOverrides[entry["bone"].GetString()] = boneOverride;
			}
		}

		if (doc.HasMember("bodySliders") && doc["bodySliders"].IsArray())
		{
			// Make sure the rig's default sliders exist so saved values have a
			// mapping to land on; unknown names are kept as mapping-less sliders
			// so the value round-trips.
			outComponent.EnsureDefaultBodySliders();
			for (const auto& entry : doc["bodySliders"].GetArray())
			{
				if (!entry.IsObject() || !entry.HasMember("name") || !entry["name"].IsString()
					|| !entry.HasMember("value") || !entry["value"].IsNumber())
					continue;

				const std::string name = entry["name"].GetString();
				BodySlider* slider = outComponent.FindSlider(name);
				if (!slider)
				{
					BodySlider created;
					created.name = name;
					outComponent.bodySliders.push_back(created);
					slider = &outComponent.bodySliders.back();
				}

				if (entry.HasMember("custom") && entry["custom"].IsBool() && entry["custom"].GetBool())
				{
					slider->customMappings = true;
					if (entry.HasMember("min") && entry["min"].IsNumber())
						slider->minValue = entry["min"].GetFloat();
					if (entry.HasMember("max") && entry["max"].IsNumber())
						slider->maxValue = entry["max"].GetFloat();
					if (slider->maxValue < slider->minValue)
						std::swap(slider->minValue, slider->maxValue);
					slider->mappings.clear();
					if (entry.HasMember("mappings") && entry["mappings"].IsArray())
					{
						for (const auto& mappingEntry : entry["mappings"].GetArray())
						{
							if (!mappingEntry.IsObject() || !mappingEntry.HasMember("bone") || !mappingEntry["bone"].IsString())
								continue;
							BodySliderBoneMapping mapping;
							mapping.boneName = mappingEntry["bone"].GetString();
							ReadVec3(mappingEntry, "positionPerUnit", mapping.positionPerUnit);
							ReadVec3(mappingEntry, "rotationDegreesPerUnit", mapping.rotationDegreesPerUnit);
							ReadVec3(mappingEntry, "scalePerUnit", mapping.scalePerUnit);
							if (mappingEntry.HasMember("counterScaleChildren") && mappingEntry["counterScaleChildren"].IsBool())
								mapping.counterScaleChildren = mappingEntry["counterScaleChildren"].GetBool();
							slider->mappings.push_back(std::move(mapping));
						}
					}
				}
				slider->value = glm::clamp(entry["value"].GetFloat(), slider->minValue, slider->maxValue);
			}
		}

		if (doc.HasMember("sockets") && doc["sockets"].IsArray())
		{
			for (const auto& entry : doc["sockets"].GetArray())
			{
				if (!entry.IsObject() || !entry.HasMember("name") || !entry["name"].IsString())
					continue;

				BoneSocket socket;
				socket.name = entry["name"].GetString();
				if (entry.HasMember("bone") && entry["bone"].IsString())
					socket.boneName = entry["bone"].GetString();
				if (entry.HasMember("mesh") && entry["mesh"].IsString())
					socket.meshPath = entry["mesh"].GetString();
				ReadVec3(entry, "positionOffset", socket.positionOffset);
				ReadVec3(entry, "rotationOffsetDegrees", socket.rotationOffsetDegrees);
				ReadVec3(entry, "scale", socket.scale);
				if (entry.HasMember("visible") && entry["visible"].IsBool())
					socket.visible = entry["visible"].GetBool();
				ReadMaterial(entry, "material", socket.material);
				outComponent.sockets.push_back(std::move(socket));
			}
		}

		return true;
	}

	bool Save(const CharacterCustomizationComponent& component, const std::string& path, const Skeleton* skeleton)
	{
		if (path.empty())
			return false;

		std::error_code ec;
		const std::filesystem::path parent = std::filesystem::path(path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent, ec);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;

		out << ToJson(component, skeleton);
		return static_cast<bool>(out);
	}

	bool Load(const std::string& path, CharacterCustomizationComponent& outComponent)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return false;

		const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (!FromJson(json, outComponent))
			return false;

		outComponent.presetPath = path;
		return true;
	}

	namespace
	{
		bool HasPresetExtension(const std::string& filename)
		{
			const std::string ext = kFileExtension;
			return filename.size() >= ext.size()
				&& filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0;
		}

		std::string ToGenericString(const std::filesystem::path& path)
		{
			return path.generic_string();
		}
	}

	std::vector<std::string> ListPresets(const std::string& directory)
	{
		std::vector<std::string> result;
		std::error_code ec;
		if (directory.empty() || !std::filesystem::is_directory(directory, ec))
			return result;

		for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
		{
			if (!entry.is_regular_file(ec))
				continue;
			const std::string filename = entry.path().filename().string();
			if (HasPresetExtension(filename))
				result.push_back(ToGenericString(entry.path()));
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	std::string PresetDisplayName(const std::string& path)
	{
		std::string name = std::filesystem::path(path).filename().string();
		if (HasPresetExtension(name))
			name.erase(name.size() - std::string(kFileExtension).size());
		return name;
	}

	std::string MakePresetPath(const std::string& directory, const std::string& name)
	{
		if (name.empty())
			return {};
		std::string filename = name;
		if (!HasPresetExtension(filename))
			filename += kFileExtension;
		return ToGenericString(std::filesystem::path(directory) / filename);
	}

	std::string Duplicate(const std::string& sourcePath, const std::string& newName)
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file(sourcePath, ec))
			return {};

		const std::string directory = ToGenericString(std::filesystem::path(sourcePath).parent_path());
		std::string target;
		if (!newName.empty())
		{
			target = MakePresetPath(directory, newName);
			if (std::filesystem::exists(target, ec))
				return {};
		}
		else
		{
			const std::string base = PresetDisplayName(sourcePath) + " Copy";
			target = MakePresetPath(directory, base);
			for (int suffix = 2; std::filesystem::exists(target, ec); ++suffix)
				target = MakePresetPath(directory, base + " " + std::to_string(suffix));
		}

		if (!std::filesystem::copy_file(sourcePath, target, std::filesystem::copy_options::none, ec) || ec)
			return {};
		return target;
	}

	std::string Rename(const std::string& sourcePath, const std::string& newName)
	{
		std::error_code ec;
		if (newName.empty() || !std::filesystem::is_regular_file(sourcePath, ec))
			return {};

		const std::string directory = ToGenericString(std::filesystem::path(sourcePath).parent_path());
		const std::string target = MakePresetPath(directory, newName);
		if (target == ToGenericString(sourcePath))
			return target;
		if (std::filesystem::exists(target, ec))
			return {};

		std::filesystem::rename(sourcePath, target, ec);
		return ec ? std::string() : target;
	}

	bool Delete(const std::string& path)
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec) || !HasPresetExtension(std::filesystem::path(path).filename().string()))
			return false;
		return std::filesystem::remove(path, ec) && !ec;
	}
}
