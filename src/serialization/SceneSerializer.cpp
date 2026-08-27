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
#include "components/CapsuleColliderComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/AudioListenerComponent.h"
#include "components/JointComponent.h"
#include "components/MeshColliderComponent.h"
#include "components/SkeletonComponent.h"
#include "core/CollisionMatrix.h"
#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/ScriptComponent.h"
#include "components/LODComponent.h"
#include "components/TerrainComponent.h"
#include "components/NavigationAgentComponent.h"
#include "components/ParticleEmitterComponent.h"
#include "components/PrefabInstanceComponent.h"
#include "core/LayerMask.h"
#include "rendering/MeshPrimitives.h"
#include "rendering/Texture.h"
#include "audio/AudioClip.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
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

		static void SerializeVec4(PrettyWriter<StringBuffer>& writer, const glm::vec4& v)
		{
			writer.StartArray();
			writer.Double(v.x);
			writer.Double(v.y);
			writer.Double(v.z);
			writer.Double(v.w);
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

		static glm::vec4 DeserializeVec4(const Value& a)
		{
			glm::vec4 v(0.0f);
			if (a.IsArray() && a.Size() >= 4)
			{
				v.x = static_cast<float>(a[0].GetDouble());
				v.y = static_cast<float>(a[1].GetDouble());
				v.z = static_cast<float>(a[2].GetDouble());
				v.w = static_cast<float>(a[3].GetDouble());
			}
			return v;
		}

		bool SaveScene(
			const ::Scene& scene,
			const std::string& path,
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& globalScripts
		)
		{
			std::string json = SaveSceneToString(scene, globalScripts);
			if (json.empty()) return false;

			std::ofstream ofs(path, std::ios::binary);
			if (!ofs)
			{
				std::cerr << "Failed to open " << path << " for writing." << std::endl;
				return false;
			}
			ofs << json;
			ofs.close();
			return true;
		}

		std::string SaveSceneToString(
			const ::Scene& scene,
			const std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>& globalScripts
		)
		{
			StringBuffer sb;
			PrettyWriter<StringBuffer> writer(sb);

			writer.StartObject();
			writer.Key("sceneVersion"); writer.Int(3);

			// Layer name registry
				writer.Key("layerNames"); writer.StartArray();
				for (int i = 0; i < MyEngine::MAX_LAYERS; ++i)
					writer.String(MyEngine::LayerMask::GetName(i).c_str());
				writer.EndArray();

				// Collision layer matrix (32 rows, each a uint32 bitmask)
				writer.Key("collisionMatrix"); writer.StartArray();
				for (int i = 0; i < MyEngine::CollisionMatrix::NUM_LAYERS; ++i)
					writer.Uint(MyEngine::CollisionMatrix::GetRows()[i]);
				writer.EndArray();

			writer.Key("entities");
			writer.StartArray();

			for (const auto& e : scene.GetEntities())
			{
				if (!e)
					continue;

				writer.StartObject();

				writer.Key("id"); writer.Uint(e->GetID());
				writer.Key("name"); writer.String(e->GetName().c_str());
				writer.Key("tag");  writer.String(e->GetTag().c_str());
				writer.Key("layer"); writer.Uint(e->GetLayer());

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
					writer.Key("position"); SerializeVec3(writer, l.position);
					writer.Key("range"); writer.Double(l.range);
					writer.Key("innerCone"); writer.Double(l.innerCone);
					writer.Key("outerCone"); writer.Double(l.outerCone);
					writer.Key("shadowBias"); writer.Double(l.shadowBias);
					writer.Key("castShadows"); writer.Bool(l.castShadows);
					writer.EndObject();
				}

				// MeshComponent
				if (e->HasComponent<MeshComponent>())
				{
					auto& mc = e->GetComponent<MeshComponent>();
					writer.Key("MeshComponent");
					writer.StartObject();
					writer.Key("assetPath"); writer.String(mc.assetPath.c_str());
					// Entities with a SkeletonComponent were loaded as skinned
					// models (see AssetManager::LoadSkinnedModel); flag this so
					// the loader reconstructs the mesh via the skinned path
					// instead of the plain static LoadModel path, which would
					// silently drop the skeleton/animation clips.
					writer.Key("isSkinned"); writer.Bool(e->HasComponent<SkeletonComponent>());
					writer.EndObject();
				}

				// LODComponent
				if (e->HasComponent<LODComponent>())
				{
					auto& lod = e->GetComponent<LODComponent>();
					writer.Key("LODComponent");
					writer.StartObject();
					writer.Key("enabled"); writer.Bool(lod.enabled);
					writer.Key("levels"); writer.StartArray();
					for (auto& lvl : lod.levels)
					{
						writer.StartObject();
						writer.Key("distance"); writer.Double(lvl.distanceThreshold);
						writer.Key("assetPath"); writer.String(lvl.assetPath.c_str());
						writer.EndObject();
					}
					writer.EndArray();
					writer.EndObject();
				}

				// TerrainComponent
				if (e->HasComponent<TerrainComponent>())
				{
					auto& tc = e->GetComponent<TerrainComponent>();
					writer.Key("TerrainComponent");
					writer.StartObject();
					writer.Key("heightmapPath");      writer.String(tc.heightmapPath.c_str());
					writer.Key("width");              writer.Double(tc.width);
					writer.Key("depth");              writer.Double(tc.depth);
					writer.Key("heightScale");        writer.Double(tc.heightScale);
					writer.Key("resolution");         writer.Int(tc.resolution);
					writer.Key("surfaceTexturePath"); writer.String(tc.surfaceTexturePath.c_str());
					writer.Key("shaderVertPath");     writer.String(tc.shaderVertPath.c_str());
					writer.Key("shaderFragPath");     writer.String(tc.shaderFragPath.c_str());
					writer.EndObject();
				}

				// AnimationComponent: playback state only. The clips/skeleton
				// themselves come back from re-loading the skinned model via
				// MeshComponent.assetPath (see above/below), since they're
				// shared_ptrs to potentially large shared data that shouldn't
				// be duplicated into every scene file.
				if (e->HasComponent<AnimationComponent>())
				{
					auto& ac = e->GetComponent<AnimationComponent>();
					writer.Key("AnimationComponent");
					writer.StartObject();
					writer.Key("activeClipIndex"); writer.Int(ac.activeClipIndex);
					writer.Key("time"); writer.Double(ac.time);
					writer.Key("playbackSpeed"); writer.Double(ac.playbackSpeed);
					writer.Key("playing"); writer.Bool(ac.playing);
					writer.Key("looping"); writer.Bool(ac.looping);
					writer.EndObject();
				}

				if (e->HasComponent<AnimationStateMachineComponent>())
				{
					auto& sm = e->GetComponent<AnimationStateMachineComponent>();
					writer.Key("AnimationStateMachineComponent");
					writer.StartObject();
					writer.Key("assetPath"); writer.String(sm.assetPath.c_str());
					writer.Key("currentStateIndex"); writer.Int(sm.currentStateIndex);
					writer.Key("pendingStateIndex"); writer.Int(sm.pendingStateIndex);
					writer.Key("currentStateTime"); writer.Double(sm.currentStateTime);
					writer.Key("autoInitialize"); writer.Bool(sm.autoInitialize);
					writer.Key("debugPauseTransitions"); writer.Bool(sm.debugPauseTransitions);
					writer.Key("parameterValues");
					writer.StartArray();
					for (const auto& value : sm.parameterValues)
					{
						writer.StartObject();
						writer.Key("floatValue"); writer.Double(value.floatValue);
						writer.Key("boolValue"); writer.Bool(value.boolValue);
						writer.Key("triggerValue"); writer.Bool(value.triggerValue);
						writer.EndObject();
					}
					writer.EndArray();
					writer.EndObject();
				}

				// Prefab instances
				if (e->HasComponent<PrefabInstanceComponent>())
				{
					auto& prefab = e->GetComponent<PrefabInstanceComponent>();
					writer.Key("PrefabInstanceComponent");
					writer.StartObject();
					writer.Key("sourcePrefabPath"); writer.String(prefab.sourcePrefabPath.c_str());
					writer.Key("sourceEntityID"); writer.Uint(prefab.sourceEntityID);
					writer.EndObject();
				}

				// NavigationAgentComponent
				if (e->HasComponent<NavigationAgentComponent>())
				{
					auto& nav = e->GetComponent<NavigationAgentComponent>();
					writer.Key("NavAgent");
					writer.StartObject();
					writer.Key("speed");           writer.Double(nav.speed);
					writer.Key("stoppingDistance"); writer.Double(nav.stoppingDistance);
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
					if (!mr.materialPath.empty())
					{
						writer.Key("materialPath"); writer.String(mr.materialPath.c_str());
					}
					else if (mr.material && !mr.material->GetPath().empty())
					{
						writer.Key("materialPath"); writer.String(mr.material->GetPath().c_str());
					}
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
						writer.Key("useCCD"); writer.Bool(rb.useCCD);
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
					writer.Key("isTrigger"); writer.Bool(box.isTrigger);
					writer.EndObject();
				}

				// Capsule Collider
				if (e->HasComponent<CapsuleColliderComponent>())
				{
					auto& capsule = e->GetComponent<CapsuleColliderComponent>();
					writer.Key("CapsuleCollider");
					writer.StartObject();
					writer.Key("pointA"); SerializeVec3(writer, capsule.pointA);
					writer.Key("pointB"); SerializeVec3(writer, capsule.pointB);
					writer.Key("radius"); writer.Double(capsule.radius);
					writer.Key("isTrigger"); writer.Bool(capsule.isTrigger);
					writer.EndObject();
				}

				if (e->HasComponent<MyEngine::CharacterControllerComponent>())
				{
					auto& controller = e->GetComponent<MyEngine::CharacterControllerComponent>();
					writer.Key("CharacterController");
					writer.StartObject();
					writer.Key("moveSpeed"); writer.Double(controller.moveSpeed);
					writer.Key("airControl"); writer.Double(controller.airControl);
					writer.Key("jumpSpeed"); writer.Double(controller.jumpSpeed);
					writer.Key("gravityScale"); writer.Double(controller.gravityScale);
					writer.Key("maxSlopeAngleDegrees"); writer.Double(controller.maxSlopeAngleDegrees);
					writer.Key("groundSnapDistance"); writer.Double(controller.groundSnapDistance);
					writer.Key("skinWidth"); writer.Double(controller.skinWidth);
					writer.Key("maxStepHeight"); writer.Double(controller.maxStepHeight);
					writer.Key("acceleration"); writer.Double(controller.acceleration);
					writer.Key("airAcceleration"); writer.Double(controller.airAcceleration);
					writer.Key("braking"); writer.Double(controller.braking);
					writer.Key("slideGravityScale"); writer.Double(controller.slideGravityScale);
					writer.Key("enableGroundSnap"); writer.Bool(controller.enableGroundSnap);
					writer.Key("orientToMovement"); writer.Bool(controller.orientToMovement);
					writer.Key("animationSpeedParameter"); writer.String(controller.animationSpeedParameter.c_str());
					writer.Key("animationGroundedParameter"); writer.String(controller.animationGroundedParameter.c_str());
					writer.Key("animationJumpTriggerParameter"); writer.String(controller.animationJumpTriggerParameter.c_str());
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
					writer.Key("isTrigger"); writer.Bool(plane.isTrigger);
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
					writer.Key("isTrigger"); writer.Bool(sphere.isTrigger);
					writer.EndObject();
				}

				// Mesh Collider
				if (e->HasComponent<MeshColliderComponent>())
				{
					auto& mesh = e->GetComponent<MeshColliderComponent>();
					writer.Key("MeshCollider");
					writer.StartObject();
					writer.Key("modelPath"); writer.String(mesh.modelPath.c_str());
					writer.Key("isTrigger"); writer.Bool(mesh.isTrigger);
					// Serialize each triangle as 9 floats [ax,ay,az, bx,by,bz, cx,cy,cz]
					writer.Key("triangles"); writer.StartArray();
					for (const auto& tri : mesh.triangles)
					{
						for (const auto& v : tri)
						{
							writer.Double(v.x);
							writer.Double(v.y);
							writer.Double(v.z);
						}
					}
					writer.EndArray();
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

				// Particle Emitter
				if (e->HasComponent<ParticleEmitterComponent>())
				{
					auto& pe = e->GetComponent<ParticleEmitterComponent>();
					writer.Key("ParticleEmitter");
					writer.StartObject();
					writer.Key("maxParticles"); writer.Int(pe.maxParticles);
					writer.Key("spawnRate"); writer.Double(pe.spawnRate);
					writer.Key("emitting"); writer.Bool(pe.emitting);
					writer.Key("shape"); writer.Int(static_cast<int>(pe.shape));
					writer.Key("shapeRadius"); writer.Double(pe.shapeRadius);
					writer.Key("shapeExtents"); SerializeVec3(writer, pe.shapeExtents);
					writer.Key("shapeHeight"); writer.Double(pe.shapeHeight);
					writer.Key("colorStart"); SerializeVec4(writer, pe.colorStart);
					writer.Key("colorEnd"); SerializeVec4(writer, pe.colorEnd);
					writer.Key("sizeStart"); writer.Double(pe.sizeStart);
					writer.Key("sizeEnd"); writer.Double(pe.sizeEnd);
					writer.Key("lifetime"); writer.Double(pe.lifetime);
					writer.Key("lifetimeVariance"); writer.Double(pe.lifetimeVariance);
					writer.Key("emitDirection"); SerializeVec3(writer, pe.emitDirection);
					writer.Key("emitSpeed"); writer.Double(pe.emitSpeed);
					writer.Key("emitSpeedVariance"); writer.Double(pe.emitSpeedVariance);
					writer.Key("spreadAngle"); writer.Double(pe.spreadAngle);
					writer.Key("gravity"); SerializeVec3(writer, pe.gravity);
					if (!pe.texturePath.empty())
					{
						writer.Key("texturePath"); writer.String(pe.texturePath.c_str());
					}
					writer.EndObject();
				}

				// Joint
				if (e->HasComponent<JointComponent>())
				{
					auto& joint = e->GetComponent<JointComponent>();
					writer.Key("Joint");
					writer.StartObject();
					writer.Key("type"); writer.Int(static_cast<int>(joint.type));
					writer.Key("connectedEntityID"); writer.Uint(joint.connectedEntityID);
					writer.Key("anchor"); SerializeVec3(writer, joint.anchor);
					writer.Key("connectedAnchor"); SerializeVec3(writer, joint.connectedAnchor);
					writer.Key("restLength"); writer.Double(joint.restLength);
					writer.Key("stiffness"); writer.Double(joint.stiffness);
					writer.Key("damping"); writer.Double(joint.damping);
					writer.Key("hingeDistance"); writer.Double(joint.hingeDistance);
					writer.Key("enabled"); writer.Bool(joint.enabled);
					writer.EndObject();
				}

				if (e->HasComponent<ScriptComponent>())
				{
					auto& sc = e->GetComponent<ScriptComponent>();
					writer.Key("Script");
					writer.StartObject();
					writer.Key("scriptPath"); writer.String(sc.scriptPath.c_str());
					writer.Key("enabled"); writer.Bool(sc.enabled);
					writer.Key("autoStart"); writer.Bool(sc.autoStart);
					writer.EndObject();
				}

				writer.EndObject();
			}

			writer.EndArray();

			writer.Key("globalScripts");
			writer.StartArray();
			for (size_t i = 0; i < globalScripts.size(); ++i)
			{
				const auto& gs = globalScripts[i];
				writer.StartObject();
				writer.Key("order"); writer.Uint(static_cast<unsigned int>(i));
				writer.Key("scriptPath"); writer.String(gs.scriptPath.c_str());
				writer.Key("enabled"); writer.Bool(gs.enabled);
				writer.Key("autoStart"); writer.Bool(gs.autoStart);
				writer.EndObject();
			}
			writer.EndArray();

			writer.EndObject();

			return std::string(sb.GetString());
		}

		bool LoadSceneFromString(
			::Scene& scene,
			const std::string& json,
			const std::shared_ptr<MyEngine::Shader>& defaultShader,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* outGlobalScripts
		)
		{
			if (json.empty()) return false;

			Document doc;
			if (doc.Parse(json.c_str()).HasParseError())
			{
				std::cerr << "Failed to parse scene JSON from string." << std::endl;
				return false;
			}

			if (!doc.HasMember("entities") || !doc["entities"].IsArray())
				return false;

			// Restore layer name registry if present
			if (doc.HasMember("layerNames") && doc["layerNames"].IsArray())
			{
				const auto& ln = doc["layerNames"].GetArray();
					for (int i = 0; i < MyEngine::MAX_LAYERS && i < static_cast<int>(ln.Size()); ++i)
						if (ln[i].IsString())
							MyEngine::LayerMask::SetName(i, ln[i].GetString());
				}

				// Restore collision layer matrix if present
				if (doc.HasMember("collisionMatrix") && doc["collisionMatrix"].IsArray())
				{
					const auto& cm = doc["collisionMatrix"].GetArray();
					for (int i = 0; i < MyEngine::CollisionMatrix::NUM_LAYERS && i < static_cast<int>(cm.Size()); ++i)
						if (cm[i].IsUint())
							MyEngine::CollisionMatrix::SetRow(i, cm[i].GetUint());
				}

				// Re-use LoadScene by writing to a temp in-memory path trick
			// actually delegate by passing json as if from a stream.
			// We do this by writing to a temp file path and delegating, but
			// to avoid disk I/O we directly duplicate the parse+load body.
			// For now call the file-based loader via a stringstream temp file.
			// (Full inline implementation below mirrors LoadScene exactly.)

			if (outGlobalScripts)
			{
				outGlobalScripts->clear();
				if (doc.HasMember("globalScripts") && doc["globalScripts"].IsArray())
				{
					std::vector<std::pair<unsigned int, MyEngine::ScriptSystem::GlobalScriptConfig>> orderedScripts;
					for (const auto& gsValue : doc["globalScripts"].GetArray())
					{
						if (!gsValue.IsObject()) continue;
						MyEngine::ScriptSystem::GlobalScriptConfig config;
						if (gsValue.HasMember("scriptPath") && gsValue["scriptPath"].IsString())
							config.scriptPath = gsValue["scriptPath"].GetString();
						if (gsValue.HasMember("enabled") && gsValue["enabled"].IsBool())
							config.enabled = gsValue["enabled"].GetBool();
						if (gsValue.HasMember("autoStart") && gsValue["autoStart"].IsBool())
							config.autoStart = gsValue["autoStart"].GetBool();
						config.requestReload = false;
						unsigned int orderIndex = static_cast<unsigned int>(orderedScripts.size());
						if (gsValue.HasMember("order") && gsValue["order"].IsUint())
							orderIndex = gsValue["order"].GetUint();
						orderedScripts.emplace_back(orderIndex, config);
					}
					std::sort(orderedScripts.begin(), orderedScripts.end(),
						[](const auto& a, const auto& b){ return a.first < b.first; });
					for (auto& [idx, cfg] : orderedScripts)
						outGlobalScripts->push_back(cfg);
				}
			}

			// Write json to a temp file then delegate to LoadScene
			// (avoids duplicating the entire entity-load body)
			const std::string tmpPath =
				(std::filesystem::temp_directory_path() / "MyEngine_inmem_load.scene").generic_string();
			{
				std::ofstream tmp(tmpPath, std::ios::binary);
				if (!tmp) return false;
				tmp << json;
			}
			// Clear global scripts from the temp load (already handled above)
			return LoadScene(scene, tmpPath, defaultShader, nullptr);
		}

		bool LoadScene(
			::Scene& scene,
			const std::string& path,
			const std::shared_ptr<MyEngine::Shader>& defaultShader,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* outGlobalScripts
		)
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

			// Full scene load replaces current scene content.
			scene.Clear();

			// Restore layer name registry if present
			if (doc.HasMember("layerNames") && doc["layerNames"].IsArray())
			{
				const auto& ln = doc["layerNames"].GetArray();
					for (int i = 0; i < MyEngine::MAX_LAYERS && i < static_cast<int>(ln.Size()); ++i)
						if (ln[i].IsString())
							MyEngine::LayerMask::SetName(i, ln[i].GetString());
				}

				// Restore collision layer matrix if present
				if (doc.HasMember("collisionMatrix") && doc["collisionMatrix"].IsArray())
				{
					const auto& cm = doc["collisionMatrix"].GetArray();
					for (int i = 0; i < MyEngine::CollisionMatrix::NUM_LAYERS && i < static_cast<int>(cm.Size()); ++i)
						if (cm[i].IsUint())
							MyEngine::CollisionMatrix::SetRow(i, cm[i].GetUint());
				}

				if (outGlobalScripts)
			{
				outGlobalScripts->clear();
				if (doc.HasMember("globalScripts") && doc["globalScripts"].IsArray())
				{
					std::vector<std::pair<unsigned int, MyEngine::ScriptSystem::GlobalScriptConfig>> orderedScripts;
					for (const auto& gsValue : doc["globalScripts"].GetArray())
					{
						if (!gsValue.IsObject())
							continue;

						MyEngine::ScriptSystem::GlobalScriptConfig config;
						if (gsValue.HasMember("scriptPath") && gsValue["scriptPath"].IsString())
							config.scriptPath = gsValue["scriptPath"].GetString();
						if (gsValue.HasMember("enabled") && gsValue["enabled"].IsBool())
							config.enabled = gsValue["enabled"].GetBool();
						if (gsValue.HasMember("autoStart") && gsValue["autoStart"].IsBool())
							config.autoStart = gsValue["autoStart"].GetBool();
						config.requestReload = false;

						unsigned int orderIndex = static_cast<unsigned int>(orderedScripts.size());
						if (gsValue.HasMember("order") && gsValue["order"].IsUint())
							orderIndex = gsValue["order"].GetUint();
						orderedScripts.emplace_back(orderIndex, config);
					}

					std::sort(
						orderedScripts.begin(),
						orderedScripts.end(),
						[](const auto& a, const auto& b)
						{
							return a.first < b.first;
						}
					);

					for (const auto& orderedScript : orderedScripts)
					{
						outGlobalScripts->push_back(orderedScript.second);
					}
				}
			}

			std::vector<std::pair<std::shared_ptr<::Entity>, uint32_t>> pendingParents;
			std::vector<std::pair<std::shared_ptr<::Entity>, uint32_t>> pendingJoints;

			for (const auto& v : doc["entities"].GetArray())
			{
				std::string name = "";
				if (v.HasMember("name") && v["name"].IsString())
					name = v["name"].GetString();

				std::shared_ptr<::Entity> ent;
				if (v.HasMember("id") && v["id"].IsUint())
					ent = scene.CreateEntityWithID(v["id"].GetUint(), name);
				else
					ent = scene.CreateEntity(name);

				if (!ent)
					continue;

				if (v.HasMember("tag")   && v["tag"].IsString())   ent->SetTag(v["tag"].GetString());
				if (v.HasMember("layer") && v["layer"].IsUint())   ent->SetLayer(v["layer"].GetUint());

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
					if (lo.HasMember("position")) l.position = DeserializeVec3(lo["position"]);
					if (lo.HasMember("range")) l.range = static_cast<float>(lo["range"].GetDouble());
					if (lo.HasMember("innerCone")) l.innerCone = static_cast<float>(lo["innerCone"].GetDouble());
					if (lo.HasMember("outerCone")) l.outerCone = static_cast<float>(lo["outerCone"].GetDouble());
					if (lo.HasMember("shadowBias")) l.shadowBias = static_cast<float>(lo["shadowBias"].GetDouble());
					if (lo.HasMember("castShadows")) l.castShadows = lo["castShadows"].GetBool();
				}

				if (v.HasMember("MeshComponent") && v["MeshComponent"].IsObject())
				{
					const auto& mco = v["MeshComponent"];
					if (mco.HasMember("assetPath") && mco["assetPath"].IsString())
					{
						std::string path = mco["assetPath"].GetString();
						bool isSkinned = mco.HasMember("isSkinned") && mco["isSkinned"].GetBool();

						if (isSkinned && !path.empty())
						{
							// Skinned models must be reloaded via LoadSkinnedModel
							// (not LoadModel) so the skeleton/animation clips come
							// back along with the mesh - otherwise the entity would
							// silently lose its bones/animation on every reload
							// (e.g. when pausing/stopping play mode).
							MyEngine::SkinnedModelData skinnedData = MyEngine::AssetManager::LoadSkinnedModel(path);
							if (!skinnedData.meshes.empty())
							{
								// Ensure skinned entities load with a skinning-capable shader.
								auto skinnedShader = MyEngine::AssetManager::LoadShader("shaders/lit_skinned.vert", "shaders/lit.frag");
								if (!skinnedShader)
									skinnedShader = defaultShader;
								MyEngine::AssetManager::AttachSkinnedModelToEntity(ent, skinnedData, skinnedShader, path);
							}
						}
						else
						{
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
							else if (path == "primitive_plane")
							{
								mesh = MyEngine::MeshPrimitives::CreatePlane();
							}
							else if (!path.empty())
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

								// Entities using a plane collider (e.g. the ground) intentionally
								// have no bounding sphere - a large auto-generated sphere here
								// would otherwise reintroduce an invisible collider that blocks
								// movement across the plane. The explicit "BoundingSphere" block
								// below still restores one if it was actually saved.
								if (!(v.HasMember("PlaneCollider") && v["PlaneCollider"].IsObject()))
								{
									auto& bs = ent->AddComponent<BoundingSphereComponent>();
									bs.center = mc.mesh->GetBoundingCenter();
									bs.radius = mc.mesh->GetBoundingRadius();
								}
							}
						}
					}
				}

				if (v.HasMember("LODComponent") && v["LODComponent"].IsObject())
				{
					const auto& lo = v["LODComponent"];
					auto& lod = ent->AddComponent<LODComponent>();
					if (lo.HasMember("enabled")) lod.enabled = lo["enabled"].GetBool();
					if (lo.HasMember("levels") && lo["levels"].IsArray())
					{
						for (auto& lvlv : lo["levels"].GetArray())
						{
							LODComponent::Level lvl;
							if (lvlv.HasMember("distance"))  lvl.distanceThreshold = static_cast<float>(lvlv["distance"].GetDouble());
							if (lvlv.HasMember("assetPath")) lvl.assetPath = lvlv["assetPath"].GetString();
							if (!lvl.assetPath.empty())
							{
								auto meshes = MyEngine::AssetManager::LoadModel(lvl.assetPath);
								if (!meshes.empty()) lvl.mesh = meshes[0];
							}
							lod.levels.push_back(std::move(lvl));
						}
					}
				}

				if (v.HasMember("TerrainComponent") && v["TerrainComponent"].IsObject())
				{
					const auto& to = v["TerrainComponent"];
					auto& tc = ent->AddComponent<TerrainComponent>();
					if (to.HasMember("heightmapPath"))      tc.heightmapPath      = to["heightmapPath"].GetString();
					if (to.HasMember("width"))              tc.width              = static_cast<float>(to["width"].GetDouble());
					if (to.HasMember("depth"))              tc.depth              = static_cast<float>(to["depth"].GetDouble());
					if (to.HasMember("heightScale"))        tc.heightScale        = static_cast<float>(to["heightScale"].GetDouble());
					if (to.HasMember("resolution"))         tc.resolution         = to["resolution"].GetInt();
					if (to.HasMember("surfaceTexturePath")) tc.surfaceTexturePath = to["surfaceTexturePath"].GetString();
					if (to.HasMember("shaderVertPath"))     tc.shaderVertPath     = to["shaderVertPath"].GetString();
					if (to.HasMember("shaderFragPath"))     tc.shaderFragPath     = to["shaderFragPath"].GetString();
					if (!tc.surfaceTexturePath.empty())
						tc.surfaceTexture = MyEngine::AssetManager::LoadTexture(tc.surfaceTexturePath);
					if (!tc.shaderVertPath.empty() && !tc.shaderFragPath.empty())
						tc.shader = MyEngine::AssetManager::LoadShader(tc.shaderVertPath, tc.shaderFragPath);
					tc.dirty = true;
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
					if (mo.HasMember("materialPath") && mo["materialPath"].IsString())
					{
						mr.materialPath = mo["materialPath"].GetString();
						mr.material = MyEngine::AssetManager::LoadMaterial(mr.materialPath);
						if (mr.material && mr.material->shader && !ent->HasComponent<AnimationComponent>())
						{
							mr.shader = mr.material->shader;
						}
					}
					if (mo.HasMember("texturePath") && mo["texturePath"].IsString())
					{
						mr.texture = MyEngine::AssetManager::LoadTexture(mo["texturePath"].GetString());
					}
					if (mo.HasMember("useTexture")) mr.useTexture = mo["useTexture"].GetBool();
				}

				// AnimationComponent playback state: only meaningful if the
				// skinned-model reload above (MeshComponent.isSkinned) already
				// attached a SkeletonComponent/AnimationComponent with the
				// clips/skeleton restored; this just overlays saved playback
				// state (current clip/time/speed/flags) onto it.
				if (v.HasMember("AnimationComponent") && v["AnimationComponent"].IsObject() && ent->HasComponent<AnimationComponent>())
				{
					auto& ac = ent->GetComponent<AnimationComponent>();
					const auto& aco = v["AnimationComponent"];
					if (aco.HasMember("activeClipIndex")) ac.activeClipIndex = aco["activeClipIndex"].GetInt();
					if (aco.HasMember("time")) ac.time = static_cast<float>(aco["time"].GetDouble());
					if (aco.HasMember("playbackSpeed")) ac.playbackSpeed = static_cast<float>(aco["playbackSpeed"].GetDouble());
					if (aco.HasMember("playing")) ac.playing = aco["playing"].GetBool();
					if (aco.HasMember("looping")) ac.looping = aco["looping"].GetBool();
				}

				if (v.HasMember("AnimationStateMachineComponent") && v["AnimationStateMachineComponent"].IsObject())
				{
					auto& sm = ent->AddComponent<AnimationStateMachineComponent>();
					const auto& smo = v["AnimationStateMachineComponent"];
					if (smo.HasMember("assetPath") && smo["assetPath"].IsString())
					{
						sm.assetPath = smo["assetPath"].GetString();
						if (!sm.assetPath.empty())
						{
							sm.stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
							if (!sm.stateMachine->LoadFromFile(sm.assetPath))
							{
								sm.stateMachine.reset();
								sm.assetPath.clear();
							}
						}
					}
					if (smo.HasMember("currentStateIndex")) sm.currentStateIndex = smo["currentStateIndex"].GetInt();
					if (smo.HasMember("pendingStateIndex")) sm.pendingStateIndex = smo["pendingStateIndex"].GetInt();
					if (smo.HasMember("currentStateTime")) sm.currentStateTime = static_cast<float>(smo["currentStateTime"].GetDouble());
					if (smo.HasMember("autoInitialize")) sm.autoInitialize = smo["autoInitialize"].GetBool();
					if (smo.HasMember("debugPauseTransitions")) sm.debugPauseTransitions = smo["debugPauseTransitions"].GetBool();
					if (smo.HasMember("parameterValues") && smo["parameterValues"].IsArray())
					{
						sm.parameterValues.clear();
						for (const auto& valueObject : smo["parameterValues"].GetArray())
						{
							if (!valueObject.IsObject())
								continue;
							AnimationStateMachineParameterValue value;
							if (valueObject.HasMember("floatValue") && valueObject["floatValue"].IsNumber())
								value.floatValue = valueObject["floatValue"].GetFloat();
							if (valueObject.HasMember("boolValue") && valueObject["boolValue"].IsBool())
								value.boolValue = valueObject["boolValue"].GetBool();
							if (valueObject.HasMember("triggerValue") && valueObject["triggerValue"].IsBool())
								value.triggerValue = valueObject["triggerValue"].GetBool();
							sm.parameterValues.push_back(value);
						}
					}
				}

				// PrefabInstanceComponent
				if (v.HasMember("PrefabInstanceComponent") && v["PrefabInstanceComponent"].IsObject())
				{
					auto& prefab = ent->AddComponent<PrefabInstanceComponent>();
					const auto& po = v["PrefabInstanceComponent"];
					if (po.HasMember("sourcePrefabPath") && po["sourcePrefabPath"].IsString())
						prefab.sourcePrefabPath = po["sourcePrefabPath"].GetString();
					if (po.HasMember("sourceEntityID") && po["sourceEntityID"].IsUint())
						prefab.sourceEntityID = po["sourceEntityID"].GetUint();
				}

				// NavigationAgentComponent
					if (v.HasMember("NavAgent") && v["NavAgent"].IsObject())
					{
						auto& nav = ent->AddComponent<NavigationAgentComponent>();
						const auto& no = v["NavAgent"];
						if (no.HasMember("speed"))           nav.speed           = static_cast<float>(no["speed"].GetDouble());
						if (no.HasMember("stoppingDistance")) nav.stoppingDistance = static_cast<float>(no["stoppingDistance"].GetDouble());
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
					if (ro.HasMember("useCCD")) rb.useCCD = ro["useCCD"].GetBool();
				}

				// Box Collider
				if (v.HasMember("BoxCollider") && v["BoxCollider"].IsObject())
				{
					auto& box = ent->AddComponent<BoxColliderComponent>();
					const auto& bo = v["BoxCollider"];
					if (bo.HasMember("center")) box.center = DeserializeVec3(bo["center"]);
					if (bo.HasMember("halfExtents")) box.halfExtents = DeserializeVec3(bo["halfExtents"]);
					if (bo.HasMember("isTrigger")) box.isTrigger = bo["isTrigger"].GetBool();
				}

				// Capsule Collider
				if (v.HasMember("CapsuleCollider") && v["CapsuleCollider"].IsObject())
				{
					auto& capsule = ent->AddComponent<CapsuleColliderComponent>();
					const auto& co = v["CapsuleCollider"];
					if (co.HasMember("pointA")) capsule.pointA = DeserializeVec3(co["pointA"]);
					if (co.HasMember("pointB")) capsule.pointB = DeserializeVec3(co["pointB"]);
					if (co.HasMember("radius")) capsule.radius = static_cast<float>(co["radius"].GetDouble());
					if (co.HasMember("isTrigger")) capsule.isTrigger = co["isTrigger"].GetBool();
				}

				if (v.HasMember("CharacterController") && v["CharacterController"].IsObject())
				{
					auto& controller = ent->AddComponent<MyEngine::CharacterControllerComponent>();
					const auto& cco = v["CharacterController"];
					if (cco.HasMember("moveSpeed")) controller.moveSpeed = static_cast<float>(cco["moveSpeed"].GetDouble());
					if (cco.HasMember("airControl")) controller.airControl = static_cast<float>(cco["airControl"].GetDouble());
					if (cco.HasMember("jumpSpeed")) controller.jumpSpeed = static_cast<float>(cco["jumpSpeed"].GetDouble());
					if (cco.HasMember("gravityScale")) controller.gravityScale = static_cast<float>(cco["gravityScale"].GetDouble());
					if (cco.HasMember("maxSlopeAngleDegrees")) controller.maxSlopeAngleDegrees = static_cast<float>(cco["maxSlopeAngleDegrees"].GetDouble());
					if (cco.HasMember("groundSnapDistance")) controller.groundSnapDistance = static_cast<float>(cco["groundSnapDistance"].GetDouble());
					if (cco.HasMember("skinWidth")) controller.skinWidth = static_cast<float>(cco["skinWidth"].GetDouble());
					if (cco.HasMember("maxStepHeight")) controller.maxStepHeight = static_cast<float>(cco["maxStepHeight"].GetDouble());
					if (cco.HasMember("acceleration")) controller.acceleration = static_cast<float>(cco["acceleration"].GetDouble());
					if (cco.HasMember("airAcceleration")) controller.airAcceleration = static_cast<float>(cco["airAcceleration"].GetDouble());
					if (cco.HasMember("braking")) controller.braking = static_cast<float>(cco["braking"].GetDouble());
					if (cco.HasMember("slideGravityScale")) controller.slideGravityScale = static_cast<float>(cco["slideGravityScale"].GetDouble());
					if (cco.HasMember("enableGroundSnap")) controller.enableGroundSnap = cco["enableGroundSnap"].GetBool();
					if (cco.HasMember("orientToMovement")) controller.orientToMovement = cco["orientToMovement"].GetBool();
					if (cco.HasMember("animationSpeedParameter") && cco["animationSpeedParameter"].IsString()) controller.animationSpeedParameter = cco["animationSpeedParameter"].GetString();
					if (cco.HasMember("animationGroundedParameter") && cco["animationGroundedParameter"].IsString()) controller.animationGroundedParameter = cco["animationGroundedParameter"].GetString();
					if (cco.HasMember("animationJumpTriggerParameter") && cco["animationJumpTriggerParameter"].IsString()) controller.animationJumpTriggerParameter = cco["animationJumpTriggerParameter"].GetString();
				}

				// Plane Collider
				if (v.HasMember("PlaneCollider") && v["PlaneCollider"].IsObject())
				{
					auto& plane = ent->AddComponent<PlaneColliderComponent>();
					const auto& po = v["PlaneCollider"];
					if (po.HasMember("normal")) plane.normal = DeserializeVec3(po["normal"]);
					if (po.HasMember("distance")) plane.distance = static_cast<float>(po["distance"].GetDouble());
					if (po.HasMember("isTrigger")) plane.isTrigger = po["isTrigger"].GetBool();
				}

				// Bounding Sphere - restored after MeshComponent (which may already have
				// added one derived from mesh bounds) so explicit saved values win.
				if (v.HasMember("BoundingSphere") && v["BoundingSphere"].IsObject())
				{
					auto& sphere = ent->AddComponent<BoundingSphereComponent>();
					const auto& so = v["BoundingSphere"];
					if (so.HasMember("center")) sphere.center = DeserializeVec3(so["center"]);
					if (so.HasMember("radius")) sphere.radius = static_cast<float>(so["radius"].GetDouble());
					if (so.HasMember("isTrigger")) sphere.isTrigger = so["isTrigger"].GetBool();
				}

				// Mesh Collider
				if (v.HasMember("MeshCollider") && v["MeshCollider"].IsObject())
				{
					auto& mesh = ent->AddComponent<MeshColliderComponent>();
					const auto& mo = v["MeshCollider"];
					if (mo.HasMember("modelPath") && mo["modelPath"].IsString())
						mesh.modelPath = mo["modelPath"].GetString();
					if (mo.HasMember("isTrigger")) mesh.isTrigger = mo["isTrigger"].GetBool();
					if (mo.HasMember("triangles") && mo["triangles"].IsArray())
					{
						const auto& ta = mo["triangles"].GetArray();
						// Each triangle is stored as 9 consecutive floats
						const int stride = 9;
						int count = static_cast<int>(ta.Size()) / stride;
						mesh.triangles.reserve(static_cast<size_t>(count));
						for (int ti = 0; ti < count; ++ti)
						{
							int base = ti * stride;
							std::array<glm::vec3, 3> tri;
							for (int vi = 0; vi < 3; ++vi)
							{
								tri[vi].x = static_cast<float>(ta[base + vi * 3 + 0].GetDouble());
								tri[vi].y = static_cast<float>(ta[base + vi * 3 + 1].GetDouble());
								tri[vi].z = static_cast<float>(ta[base + vi * 3 + 2].GetDouble());
							}
							mesh.triangles.push_back(tri);
						}
						mesh.RebuildAABB();
					}
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

				// Particle Emitter
				if (v.HasMember("ParticleEmitter") && v["ParticleEmitter"].IsObject())
				{
					auto& pe = ent->AddComponent<ParticleEmitterComponent>();
					const auto& po = v["ParticleEmitter"];
					if (po.HasMember("maxParticles")) pe.maxParticles = po["maxParticles"].GetInt();
					if (po.HasMember("spawnRate")) pe.spawnRate = static_cast<float>(po["spawnRate"].GetDouble());
					if (po.HasMember("emitting")) pe.emitting = po["emitting"].GetBool();
					if (po.HasMember("shape")) pe.shape = static_cast<ParticleEmitterComponent::EmissionShape>(po["shape"].GetInt());
					if (po.HasMember("shapeRadius")) pe.shapeRadius = static_cast<float>(po["shapeRadius"].GetDouble());
					if (po.HasMember("shapeExtents")) pe.shapeExtents = DeserializeVec3(po["shapeExtents"]);
					if (po.HasMember("shapeHeight")) pe.shapeHeight = static_cast<float>(po["shapeHeight"].GetDouble());
					if (po.HasMember("colorStart")) pe.colorStart = DeserializeVec4(po["colorStart"]);
					if (po.HasMember("colorEnd")) pe.colorEnd = DeserializeVec4(po["colorEnd"]);
					if (po.HasMember("sizeStart")) pe.sizeStart = static_cast<float>(po["sizeStart"].GetDouble());
					if (po.HasMember("sizeEnd")) pe.sizeEnd = static_cast<float>(po["sizeEnd"].GetDouble());
					if (po.HasMember("lifetime")) pe.lifetime = static_cast<float>(po["lifetime"].GetDouble());
					if (po.HasMember("lifetimeVariance")) pe.lifetimeVariance = static_cast<float>(po["lifetimeVariance"].GetDouble());
					if (po.HasMember("emitDirection")) pe.emitDirection = DeserializeVec3(po["emitDirection"]);
					if (po.HasMember("emitSpeed")) pe.emitSpeed = static_cast<float>(po["emitSpeed"].GetDouble());
					if (po.HasMember("emitSpeedVariance")) pe.emitSpeedVariance = static_cast<float>(po["emitSpeedVariance"].GetDouble());
					if (po.HasMember("spreadAngle")) pe.spreadAngle = static_cast<float>(po["spreadAngle"].GetDouble());
					if (po.HasMember("gravity")) pe.gravity = DeserializeVec3(po["gravity"]);
					if (po.HasMember("texturePath") && po["texturePath"].IsString())
						pe.texturePath = po["texturePath"].GetString();
					pe.poolDirty = true;
				}

				// Audio Listener
				if (v.HasMember("AudioListener") && v["AudioListener"].IsObject())
				{
					auto& al = ent->AddComponent<AudioListenerComponent>();
					const auto& alo = v["AudioListener"];
					if (alo.HasMember("isPrimary")) al.isPrimary = alo["isPrimary"].GetBool();
					if (alo.HasMember("gain")) al.gain = static_cast<float>(alo["gain"].GetDouble());
				}

				// Joint reference IDs can be restored directly because scene loading
				// now preserves serialized entity IDs.
				if (v.HasMember("Joint") && v["Joint"].IsObject())
				{
					auto& joint = ent->AddComponent<JointComponent>();
					const auto& jo = v["Joint"];
					if (jo.HasMember("type")) joint.type = static_cast<JointType>(jo["type"].GetInt());
					if (jo.HasMember("anchor")) joint.anchor = DeserializeVec3(jo["anchor"]);
					if (jo.HasMember("connectedAnchor")) joint.connectedAnchor = DeserializeVec3(jo["connectedAnchor"]);
					if (jo.HasMember("restLength")) joint.restLength = static_cast<float>(jo["restLength"].GetDouble());
					if (jo.HasMember("stiffness")) joint.stiffness = static_cast<float>(jo["stiffness"].GetDouble());
					if (jo.HasMember("damping")) joint.damping = static_cast<float>(jo["damping"].GetDouble());
					if (jo.HasMember("hingeDistance")) joint.hingeDistance = static_cast<float>(jo["hingeDistance"].GetDouble());
					if (jo.HasMember("enabled")) joint.enabled = jo["enabled"].GetBool();
					if (jo.HasMember("connectedEntityID") && jo["connectedEntityID"].IsUint())
						pendingJoints.emplace_back(ent, jo["connectedEntityID"].GetUint());
				}

				if (v.HasMember("Script") && v["Script"].IsObject())
				{
					auto& sc = ent->AddComponent<ScriptComponent>();
					const auto& so = v["Script"];
					if (so.HasMember("scriptPath") && so["scriptPath"].IsString()) sc.scriptPath = so["scriptPath"].GetString();
					if (so.HasMember("enabled")) sc.enabled = so["enabled"].GetBool();
					if (so.HasMember("autoStart")) sc.autoStart = so["autoStart"].GetBool();
				}
			}

			for (auto& [ent, savedParentID] : pendingParents)
			{
				if (!ent || !ent->HasComponent<TransformComponent>())
					continue;

				ent->GetComponent<TransformComponent>().parentID =
					scene.GetEntityByID(savedParentID) ? savedParentID : 0;
			}

			for (auto& [ent, savedConnectedID] : pendingJoints)
			{
				if (!ent || !ent->HasComponent<JointComponent>())
					continue;

				ent->GetComponent<JointComponent>().connectedEntityID =
					scene.GetEntityByID(savedConnectedID) ? savedConnectedID : 0;
			}

					return true;
					}

							// -------------------------------------------------------------------
							// Prefab helpers
							// -------------------------------------------------------------------
							bool SavePrefab(
								const ::Scene& sourceScene,
								::Entity* rootEntity,
								const std::string& path
							)
							{
								if (!rootEntity)
									return false;

								const uint32_t rootID = rootEntity->GetID();
								std::unordered_set<uint32_t> subtreeIDs;
								std::queue<uint32_t> pending;
								pending.push(rootID);

								while (!pending.empty())
								{
									const uint32_t currentID = pending.front();
									pending.pop();
									if (!subtreeIDs.insert(currentID).second)
										continue;

									for (const auto& candidate : sourceScene.GetEntities())
									{
										if (!candidate || !candidate->HasComponent<TransformComponent>())
											continue;
										if (candidate->GetComponent<TransformComponent>().parentID == currentID)
											pending.push(candidate->GetID());
									}
								}

								::Scene prefabScene;
								std::vector<uint32_t> orderedIDs;
								orderedIDs.reserve(subtreeIDs.size());
								orderedIDs.push_back(rootID);
								for (const auto& e : sourceScene.GetEntities())
								{
									if (!e || e->GetID() == rootID)
										continue;
									if (subtreeIDs.find(e->GetID()) != subtreeIDs.end())
										orderedIDs.push_back(e->GetID());
								}

								for (const uint32_t id : orderedIDs)
								{
									auto source = sourceScene.GetEntitySharedByID(id);
									if (!source)
										continue;

									auto clone = prefabScene.CreateEntityWithID(source->GetID(), source->GetName());
									if (!clone)
										continue;

									clone->SetTag(source->GetTag());
									clone->SetLayer(source->GetLayer());

								#define COPY_COMPONENT(T) \
									if (source->HasComponent<T>()) clone->AddComponent<T>() = source->GetComponent<T>()
									COPY_COMPONENT(TransformComponent);
									COPY_COMPONENT(CameraComponent);
									COPY_COMPONENT(LightComponent);
									COPY_COMPONENT(MeshComponent);
									COPY_COMPONENT(MeshRendererComponent);
									COPY_COMPONENT(BoundingSphereComponent);
									COPY_COMPONENT(RigidbodyComponent);
									COPY_COMPONENT(BoxColliderComponent);
									COPY_COMPONENT(CapsuleColliderComponent);
									COPY_COMPONENT(CharacterControllerComponent);
									COPY_COMPONENT(PlaneColliderComponent);
									COPY_COMPONENT(AudioSourceComponent);
									COPY_COMPONENT(AudioListenerComponent);
									COPY_COMPONENT(JointComponent);
									COPY_COMPONENT(MeshColliderComponent);
									COPY_COMPONENT(SkeletonComponent);
									COPY_COMPONENT(AnimationComponent);
									COPY_COMPONENT(AnimationStateMachineComponent);
									COPY_COMPONENT(ScriptComponent);
									COPY_COMPONENT(LODComponent);
									COPY_COMPONENT(TerrainComponent);
									COPY_COMPONENT(NavigationAgentComponent);
									COPY_COMPONENT(ParticleEmitterComponent);
								#undef COPY_COMPONENT

									if (clone->HasComponent<TransformComponent>())
									{
										auto& t = clone->GetComponent<TransformComponent>();
										if (t.parentID == rootID)
											t.parentID = 0;
										else if (subtreeIDs.find(t.parentID) == subtreeIDs.end())
											t.parentID = 0;
									}

									if (clone->HasComponent<JointComponent>())
									{
										auto& joint = clone->GetComponent<JointComponent>();
										if (subtreeIDs.find(joint.connectedEntityID) == subtreeIDs.end())
											joint.connectedEntityID = 0;
									}
								}

								return SaveScene(prefabScene, path);
							}

							::Entity* SpawnPrefab(
								::Scene& scene,
								const std::string& path,
								const std::shared_ptr<MyEngine::Shader>& defaultShader
							)
							{
								::Scene prefabScene;
								if (!LoadScene(prefabScene, path, defaultShader))
									return nullptr;

								auto& prefabEntities = prefabScene.GetEntities();
								if (prefabEntities.empty())
									return nullptr;

								std::unordered_set<uint32_t> sourceIDs;
								for (const auto& source : prefabEntities)
									if (source) sourceIDs.insert(source->GetID());

								auto isRootCandidate = [&](const std::shared_ptr<::Entity>& candidate) -> bool
								{
									if (!candidate)
										return false;
									if (!candidate->HasComponent<TransformComponent>())
										return true;
									const auto& t = candidate->GetComponent<TransformComponent>();
									return t.parentID == 0 || sourceIDs.find(t.parentID) == sourceIDs.end();
								};

								std::shared_ptr<::Entity> sourceRoot;
								for (const auto& source : prefabEntities)
								{
									if (isRootCandidate(source))
									{
										sourceRoot = source;
										break;
									}
								}
								if (!sourceRoot)
									sourceRoot = prefabEntities.front();

								std::unordered_map<uint32_t, std::shared_ptr<::Entity>> spawnedBySourceID;
								spawnedBySourceID.reserve(prefabEntities.size());

								for (const auto& source : prefabEntities)
								{
									if (!source)
										continue;

									auto spawned = scene.CreateEntity(source->GetName());
									if (!spawned)
										continue;

										spawned->SetTag(source->GetTag());
										spawned->SetLayer(source->GetLayer());

										auto& prefabInstance = spawned->AddComponent<PrefabInstanceComponent>();
										prefabInstance.sourcePrefabPath = path;
										prefabInstance.sourceEntityID = source->GetID();

									#define COPY_COMPONENT(T) \
									if (source->HasComponent<T>()) spawned->AddComponent<T>() = source->GetComponent<T>()
									COPY_COMPONENT(TransformComponent);
									COPY_COMPONENT(CameraComponent);
									COPY_COMPONENT(LightComponent);
									COPY_COMPONENT(MeshComponent);
									COPY_COMPONENT(MeshRendererComponent);
									COPY_COMPONENT(BoundingSphereComponent);
									COPY_COMPONENT(RigidbodyComponent);
									COPY_COMPONENT(BoxColliderComponent);
									COPY_COMPONENT(CapsuleColliderComponent);
									COPY_COMPONENT(CharacterControllerComponent);
									COPY_COMPONENT(PlaneColliderComponent);
									COPY_COMPONENT(AudioSourceComponent);
									COPY_COMPONENT(AudioListenerComponent);
									COPY_COMPONENT(JointComponent);
									COPY_COMPONENT(MeshColliderComponent);
									COPY_COMPONENT(SkeletonComponent);
									COPY_COMPONENT(AnimationComponent);
									COPY_COMPONENT(AnimationStateMachineComponent);
									COPY_COMPONENT(ScriptComponent);
									COPY_COMPONENT(LODComponent);
									COPY_COMPONENT(TerrainComponent);
									COPY_COMPONENT(NavigationAgentComponent);
									COPY_COMPONENT(ParticleEmitterComponent);
								#undef COPY_COMPONENT

									spawnedBySourceID[source->GetID()] = spawned;
								}

								for (const auto& source : prefabEntities)
								{
									if (!source)
										continue;

									auto itSpawned = spawnedBySourceID.find(source->GetID());
									if (itSpawned == spawnedBySourceID.end() || !itSpawned->second)
										continue;

									auto& spawned = itSpawned->second;
									if (spawned->HasComponent<TransformComponent>() && source->HasComponent<TransformComponent>())
									{
										auto& dst = spawned->GetComponent<TransformComponent>();
										const auto& src = source->GetComponent<TransformComponent>();
										auto parentIt = spawnedBySourceID.find(src.parentID);
										dst.parentID = (parentIt != spawnedBySourceID.end() && parentIt->second)
											? parentIt->second->GetID()
											: 0;
									}

									if (spawned->HasComponent<JointComponent>() && source->HasComponent<JointComponent>())
									{
										auto& dst = spawned->GetComponent<JointComponent>();
										const auto& src = source->GetComponent<JointComponent>();
										auto connectedIt = spawnedBySourceID.find(src.connectedEntityID);
										dst.connectedEntityID = (connectedIt != spawnedBySourceID.end() && connectedIt->second)
											? connectedIt->second->GetID()
											: 0;
									}
								}

								auto rootIt = spawnedBySourceID.find(sourceRoot->GetID());
								if (rootIt == spawnedBySourceID.end() || !rootIt->second)
									return nullptr;

								return rootIt->second.get();
							}
						}
					}
