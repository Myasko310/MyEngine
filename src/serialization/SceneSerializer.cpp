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
#include "components/RigidbodyComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/AudioListenerComponent.h"
#include "rendering/MeshPrimitives.h"
#include "rendering/Texture.h"
#include "audio/AudioClip.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

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
					writer.Key("parentID"); writer.Uint(t.parentID);
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
					writer.Key("useTexture"); writer.Bool(mr.useTexture);
					if (mr.texture)
					{
						writer.Key("texturePath"); writer.String(mr.texture->GetPath().c_str());
					}
					if (mr.shader)
					{
						writer.Key("shaderVertexPath"); writer.String(mr.shader->GetVertexPath().c_str());
						writer.Key("shaderFragmentPath"); writer.String(mr.shader->GetFragmentPath().c_str());
					}
					writer.EndObject();
				}

				// Rigidbody
				if (e->HasComponent<MyEngine::RigidbodyComponent>())
				{
					auto& rb = e->GetComponent<MyEngine::RigidbodyComponent>();
					writer.Key("Rigidbody");
					writer.StartObject();
					writer.Key("velocity"); SerializeVec3(writer, rb.velocity);
					writer.Key("acceleration"); SerializeVec3(writer, rb.acceleration);
					writer.Key("mass"); writer.Double(rb.mass);
					writer.Key("drag"); writer.Double(rb.drag);
					writer.Key("bounciness"); writer.Double(rb.bounciness);
					writer.Key("useGravity"); writer.Bool(rb.useGravity);
					writer.Key("gravityScale"); writer.Double(rb.gravityScale);
					writer.Key("isKinematic"); writer.Bool(rb.isKinematic);
					writer.Key("freezePositionX"); writer.Bool(rb.freezePositionX);
					writer.Key("freezePositionY"); writer.Bool(rb.freezePositionY);
					writer.Key("freezePositionZ"); writer.Bool(rb.freezePositionZ);
					writer.EndObject();
				}

				// Box Collider
				if (e->HasComponent<BoxColliderComponent>())
				{
					auto& box = e->GetComponent<BoxColliderComponent>();
					writer.Key("BoxCollider");
					writer.StartObject();
					writer.Key("center"); SerializeVec3(writer, box.center);
					writer.Key("halfExtents"); SerializeVec3(writer, box.halfExtents);
					writer.EndObject();
				}

				// Plane Collider
				if (e->HasComponent<PlaneColliderComponent>())
				{
					auto& plane = e->GetComponent<PlaneColliderComponent>();
					writer.Key("PlaneCollider");
					writer.StartObject();
					writer.Key("normal"); SerializeVec3(writer, plane.normal);
					writer.Key("distance"); writer.Double(plane.distance);
					writer.EndObject();
				}

				// Bounding Sphere
				if (e->HasComponent<BoundingSphereComponent>())
				{
					auto& sphere = e->GetComponent<BoundingSphereComponent>();
					writer.Key("BoundingSphere");
					writer.StartObject();
					writer.Key("center"); SerializeVec3(writer, sphere.center);
					writer.Key("radius"); writer.Double(sphere.radius);
					writer.EndObject();
				}

				// Audio Source
				if (e->HasComponent<AudioSourceComponent>())
				{
					auto& as = e->GetComponent<AudioSourceComponent>();
					writer.Key("AudioSource");
					writer.StartObject();
					writer.Key("clipPath"); writer.String(as.clipPath.c_str());
					writer.Key("volume"); writer.Double(as.volume);
					writer.Key("pitch"); writer.Double(as.pitch);
					writer.Key("loop"); writer.Bool(as.loop);
					writer.Key("autoPlay"); writer.Bool(as.autoPlay);
					writer.Key("spatial"); writer.Bool(as.spatial);
					writer.Key("minDistance"); writer.Double(as.minDistance);
					writer.Key("maxDistance"); writer.Double(as.maxDistance);
					writer.EndObject();
				}

				// Audio Listener
				if (e->HasComponent<AudioListenerComponent>())
				{
					auto& al = e->GetComponent<AudioListenerComponent>();
					writer.Key("AudioListener");
					writer.StartObject();
					writer.Key("isPrimary"); writer.Bool(al.isPrimary);
					writer.Key("gain"); writer.Double(al.gain);
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

		bool LoadScene(::Scene& scene, const std::string& path, const std::shared_ptr<MyEngine::Shader>& defaultShader)
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

			// Entity IDs are reassigned on load; remember the saved id for each
			// loaded entity so parent references can be remapped afterwards.
			std::unordered_map<uint32_t, uint32_t> savedToNewID;
			std::vector<std::pair<std::shared_ptr<::Entity>, uint32_t>> pendingParents;

			for (const auto& v : doc["entities"].GetArray())
			{
				std::string name = "";
				if (v.HasMember("name") && v["name"].IsString())
					name = v["name"].GetString();

				auto ent = scene.CreateEntity(name);

				if (v.HasMember("id") && v["id"].IsUint())
					savedToNewID[v["id"].GetUint()] = ent->GetID();

				if (v.HasMember("Transform") && v["Transform"].IsObject())
				{
					auto& t = ent->AddComponent<TransformComponent>();
					const auto& to = v["Transform"];
					if (to.HasMember("position")) t.position = DeserializeVec3(to["position"]);
					if (to.HasMember("rotation")) t.rotation = DeserializeVec3(to["rotation"]);
					if (to.HasMember("scale")) t.scale = DeserializeVec3(to["scale"]);
					if (to.HasMember("parentID") && to["parentID"].IsUint() && to["parentID"].GetUint() != 0)
						pendingParents.emplace_back(ent, to["parentID"].GetUint());
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

						// Primitive meshes (created via the editor's Create menu) are not
						// real files on disk - they use sentinel asset paths and must be
						// reconstructed procedurally instead of loaded via AssetManager.
						std::shared_ptr<MyEngine::Mesh> mesh;
						if (path == "primitive_cube")
						{
							mesh = MyEngine::MeshPrimitives::CreateCube();
						}
						else if (path == "primitive_sphere")
						{
							mesh = MyEngine::MeshPrimitives::CreateSphere();
						}
						else
						{
							auto meshes = MyEngine::AssetManager::LoadModel(path);
							if (!meshes.empty())
								mesh = meshes[0];
						}

						if (mesh)
						{
							auto& mc = ent->AddComponent<MeshComponent>();
							mc.mesh = mesh;
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
					if (mo.HasMember("shaderVertexPath") && mo["shaderVertexPath"].IsString() &&
						mo.HasMember("shaderFragmentPath") && mo["shaderFragmentPath"].IsString())
					{
						mr.shader = MyEngine::AssetManager::LoadShader(mo["shaderVertexPath"].GetString(), mo["shaderFragmentPath"].GetString());
					}
					else
					{
						// Older scene files (or entities saved without a shader) fall back
						// to the caller-supplied default shader.
						mr.shader = defaultShader;
					}
					if (mo.HasMember("visible")) mr.visible = mo["visible"].GetBool();
					if (mo.HasMember("albedo")) mr.albedo = DeserializeVec3(mo["albedo"]);
					if (mo.HasMember("shininess")) mr.shininess = static_cast<float>(mo["shininess"].GetDouble());
					if (mo.HasMember("texturePath") && mo["texturePath"].IsString())
					{
						mr.texture = MyEngine::AssetManager::LoadTexture(mo["texturePath"].GetString());
					}
					if (mo.HasMember("useTexture")) mr.useTexture = mo["useTexture"].GetBool();
				}

				// Rigidbody (must exist as a Component-derived type; add then populate fields)
				if (v.HasMember("Rigidbody") && v["Rigidbody"].IsObject())
				{
					auto& rb = ent->AddComponent<MyEngine::RigidbodyComponent>();
					const auto& ro = v["Rigidbody"];
					if (ro.HasMember("velocity")) rb.velocity = DeserializeVec3(ro["velocity"]);
					if (ro.HasMember("acceleration")) rb.acceleration = DeserializeVec3(ro["acceleration"]);
					if (ro.HasMember("mass")) rb.mass = static_cast<float>(ro["mass"].GetDouble());
					if (ro.HasMember("drag")) rb.drag = static_cast<float>(ro["drag"].GetDouble());
					if (ro.HasMember("bounciness")) rb.bounciness = static_cast<float>(ro["bounciness"].GetDouble());
					if (ro.HasMember("useGravity")) rb.useGravity = ro["useGravity"].GetBool();
					if (ro.HasMember("gravityScale")) rb.gravityScale = static_cast<float>(ro["gravityScale"].GetDouble());
					if (ro.HasMember("isKinematic")) rb.isKinematic = ro["isKinematic"].GetBool();
					if (ro.HasMember("freezePositionX")) rb.freezePositionX = ro["freezePositionX"].GetBool();
					if (ro.HasMember("freezePositionY")) rb.freezePositionY = ro["freezePositionY"].GetBool();
					if (ro.HasMember("freezePositionZ")) rb.freezePositionZ = ro["freezePositionZ"].GetBool();
				}

				// Box Collider
				if (v.HasMember("BoxCollider") && v["BoxCollider"].IsObject())
				{
					auto& box = ent->AddComponent<BoxColliderComponent>();
					const auto& bo = v["BoxCollider"];
					if (bo.HasMember("center")) box.center = DeserializeVec3(bo["center"]);
					if (bo.HasMember("halfExtents")) box.halfExtents = DeserializeVec3(bo["halfExtents"]);
				}

				// Plane Collider
				if (v.HasMember("PlaneCollider") && v["PlaneCollider"].IsObject())
				{
					auto& plane = ent->AddComponent<PlaneColliderComponent>();
					const auto& po = v["PlaneCollider"];
					if (po.HasMember("normal")) plane.normal = DeserializeVec3(po["normal"]);
					if (po.HasMember("distance")) plane.distance = static_cast<float>(po["distance"].GetDouble());
				}

				// Bounding Sphere - restored after MeshComponent (which may already have
				// added one derived from mesh bounds) so explicit saved values win.
				if (v.HasMember("BoundingSphere") && v["BoundingSphere"].IsObject())
				{
					auto& sphere = ent->AddComponent<BoundingSphereComponent>();
					const auto& so = v["BoundingSphere"];
					if (so.HasMember("center")) sphere.center = DeserializeVec3(so["center"]);
					if (so.HasMember("radius")) sphere.radius = static_cast<float>(so["radius"].GetDouble());
				}

				// Audio Source
				if (v.HasMember("AudioSource") && v["AudioSource"].IsObject())
				{
					auto& as = ent->AddComponent<AudioSourceComponent>();
					const auto& ao = v["AudioSource"];
					if (ao.HasMember("clipPath") && ao["clipPath"].IsString())
					{
						as.clipPath = ao["clipPath"].GetString();
						if (!as.clipPath.empty())
							as.clip = MyEngine::AssetManager::LoadAudioClip(as.clipPath);
					}
					if (ao.HasMember("volume")) as.volume = static_cast<float>(ao["volume"].GetDouble());
					if (ao.HasMember("pitch")) as.pitch = static_cast<float>(ao["pitch"].GetDouble());
					if (ao.HasMember("loop")) as.loop = ao["loop"].GetBool();
					if (ao.HasMember("autoPlay")) as.autoPlay = ao["autoPlay"].GetBool();
					if (ao.HasMember("spatial")) as.spatial = ao["spatial"].GetBool();
					if (ao.HasMember("minDistance")) as.minDistance = static_cast<float>(ao["minDistance"].GetDouble());
					if (ao.HasMember("maxDistance")) as.maxDistance = static_cast<float>(ao["maxDistance"].GetDouble());
				}

				// Audio Listener
				if (v.HasMember("AudioListener") && v["AudioListener"].IsObject())
				{
					auto& al = ent->AddComponent<AudioListenerComponent>();
					const auto& alo = v["AudioListener"];
					if (alo.HasMember("isPrimary")) al.isPrimary = alo["isPrimary"].GetBool();
					if (alo.HasMember("gain")) al.gain = static_cast<float>(alo["gain"].GetDouble());
				}
			}

			// Remap parent references from saved IDs to newly assigned IDs.
			for (auto& [ent, savedParentID] : pendingParents)
			{
				auto it = savedToNewID.find(savedParentID);
				if (it != savedToNewID.end() && ent->HasComponent<TransformComponent>())
					ent->GetComponent<TransformComponent>().parentID = it->second;
			}

			return true;
		}
	}
}
