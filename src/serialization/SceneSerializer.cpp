#include "serialization/SceneSerializer.h"

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"
#include "components/CameraComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "core/AssetManager.h"
#include "components/BoundingSphereComponent.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <fstream>
#include <iostream>

using namespace rapidjson;

namespace MyEngine
{
	namespace Serialization
	{
		static void SerializeVec3(PrettyWriter<StringBuffer>& writer, const glm::vec3& v)
		{
			writer.StartArray();
			writer.Double(v.x);
			writer.Double(v.y);
			writer.Double(v.z);
			writer.EndArray();
		}

		static glm::vec3 DeserializeVec3(const Value& a)
		{
			glm::vec3 v(0.0f);
			if (a.IsArray() && a.Size() >= 3)
			{
				v.x = static_cast<float>(a[0].GetDouble());
				v.y = static_cast<float>(a[1].GetDouble());
				v.z = static_cast<float>(a[2].GetDouble());
			}
			return v;
		}

		bool SaveScene(const ::Scene& scene, const std::string& path)
		{
			StringBuffer sb;
			PrettyWriter<StringBuffer> writer(sb);

			writer.StartObject();
			writer.Key("entities");
			writer.StartArray();

			for (const auto& e : scene.GetEntities())
			{
				if (!e)
					continue;

				writer.StartObject();

				writer.Key("id"); writer.Uint(e->GetID());
				writer.Key("name"); writer.String(e->GetName().c_str());

				// Transform
				if (e->HasComponent<TransformComponent>())
				{
					auto& t = e->GetComponent<TransformComponent>();
					writer.Key("Transform");
					writer.StartObject();
					writer.Key("position"); SerializeVec3(writer, t.position);
					writer.Key("rotation"); SerializeVec3(writer, t.rotation);
					writer.Key("scale"); SerializeVec3(writer, t.scale);
					writer.EndObject();
				}

				// Camera
				if (e->HasComponent<CameraComponent>())
				{
					auto& c = e->GetComponent<CameraComponent>();
					writer.Key("Camera");
					writer.StartObject();
					writer.Key("isPrimary"); writer.Bool(c.isPrimary);
					writer.Key("fov"); writer.Double(c.fov);
					writer.Key("nearPlane"); writer.Double(c.nearPlane);
					writer.Key("farPlane"); writer.Double(c.farPlane);
					writer.EndObject();
				}

				// Light
				if (e->HasComponent<LightComponent>())
				{
					auto& l = e->GetComponent<LightComponent>();
					writer.Key("Light");
					writer.StartObject();
					writer.Key("type"); writer.Int(static_cast<int>(l.type));
					writer.Key("color"); SerializeVec3(writer, l.color);
					writer.Key("intensity"); writer.Double(l.intensity);
					writer.Key("direction"); SerializeVec3(writer, l.direction);
					writer.EndObject();
				}

				// MeshComponent
				if (e->HasComponent<MeshComponent>())
				{
					auto& mc = e->GetComponent<MeshComponent>();
					writer.Key("MeshComponent");
					writer.StartObject();
					writer.Key("assetPath"); writer.String(mc.assetPath.c_str());
					writer.EndObject();
				}

				// MeshRenderer
				if (e->HasComponent<MeshRendererComponent>())
				{
					auto& mr = e->GetComponent<MeshRendererComponent>();
					writer.Key("MeshRenderer");
					writer.StartObject();
					writer.Key("visible"); writer.Bool(mr.visible);
					writer.Key("albedo"); SerializeVec3(writer, mr.albedo);
					writer.Key("shininess"); writer.Double(mr.shininess);
					writer.EndObject();
				}

				writer.EndObject();
			}

			writer.EndArray();
			writer.EndObject();

			std::ofstream ofs(path, std::ios::binary);
			if (!ofs)
			{
				std::cerr << "Failed to open " << path << " for writing." << std::endl;
				return false;
			}

			ofs << sb.GetString();
			ofs.close();

			return true;
		}

		bool LoadScene(::Scene& scene, const std::string& path)
		{
			std::ifstream ifs(path);
			if (!ifs)
			{
				std::cerr << "Failed to open " << path << " for reading." << std::endl;
				return false;
			}

			std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			ifs.close();

			Document doc;
			if (doc.Parse(content.c_str()).HasParseError())
			{
				std::cerr << "Failed to parse scene JSON: " << path << std::endl;
				return false;
			}

			if (!doc.HasMember("entities") || !doc["entities"].IsArray())
				return false;

			for (const auto& v : doc["entities"].GetArray())
			{
				std::string name = "";
				if (v.HasMember("name") && v["name"].IsString())
					name = v["name"].GetString();

				auto ent = scene.CreateEntity(name);

				if (v.HasMember("Transform") && v["Transform"].IsObject())
				{
					auto& t = ent->AddComponent<TransformComponent>();
					const auto& to = v["Transform"];
					if (to.HasMember("position")) t.position = DeserializeVec3(to["position"]);
					if (to.HasMember("rotation")) t.rotation = DeserializeVec3(to["rotation"]);
					if (to.HasMember("scale")) t.scale = DeserializeVec3(to["scale"]);
				}

				if (v.HasMember("Camera") && v["Camera"].IsObject())
				{
					auto& c = ent->AddComponent<CameraComponent>();
					const auto& co = v["Camera"];
					if (co.HasMember("isPrimary")) c.isPrimary = co["isPrimary"].GetBool();
					if (co.HasMember("fov")) c.fov = static_cast<float>(co["fov"].GetDouble());
					if (co.HasMember("nearPlane")) c.nearPlane = static_cast<float>(co["nearPlane"].GetDouble());
					if (co.HasMember("farPlane")) c.farPlane = static_cast<float>(co["farPlane"].GetDouble());
				}

				if (v.HasMember("Light") && v["Light"].IsObject())
				{
					auto& l = ent->AddComponent<LightComponent>();
					const auto& lo = v["Light"];
					if (lo.HasMember("type")) l.type = static_cast<LightComponent::Type>(lo["type"].GetInt());
					if (lo.HasMember("color")) l.color = DeserializeVec3(lo["color"]);
					if (lo.HasMember("intensity")) l.intensity = static_cast<float>(lo["intensity"].GetDouble());
					if (lo.HasMember("direction")) l.direction = DeserializeVec3(lo["direction"]);
				}

				if (v.HasMember("MeshComponent") && v["MeshComponent"].IsObject())
				{
					const auto& mco = v["MeshComponent"];
					if (mco.HasMember("assetPath") && mco["assetPath"].IsString())
					{
						std::string path = mco["assetPath"].GetString();
						auto meshes = MyEngine::AssetManager::LoadModel(path);
						if (!meshes.empty())
						{
							auto& mc = ent->AddComponent<MeshComponent>();
							mc.mesh = meshes[0];
							mc.assetPath = path;

							auto& bs = ent->AddComponent<BoundingSphereComponent>();
							bs.center = mc.mesh->GetBoundingCenter();
							bs.radius = mc.mesh->GetBoundingRadius();
						}
					}
				}

				if (v.HasMember("MeshRenderer") && v["MeshRenderer"].IsObject())
				{
					auto& mr = ent->AddComponent<MeshRendererComponent>();
					const auto& mo = v["MeshRenderer"];
					if (mo.HasMember("visible")) mr.visible = mo["visible"].GetBool();
					if (mo.HasMember("albedo")) mr.albedo = DeserializeVec3(mo["albedo"]);
					if (mo.HasMember("shininess")) mr.shininess = static_cast<float>(mo["shininess"].GetDouble());
				}
			}

			return true;
		}
	}
}
