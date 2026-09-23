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
#include "components/MovingPlatformComponent.h"
#include "components/NavigationAgentComponent.h"
#include "components/ParticleEmitterComponent.h"
#include "components/PrefabInstanceComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/BossAIComponent.h"
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

		static const char* ToStateMachineParameterTypeString(MyEngine::AnimationStateMachineParameterType type)
		{
			switch (type)
			{
			case MyEngine::AnimationStateMachineParameterType::Bool: return "Bool";
			case MyEngine::AnimationStateMachineParameterType::Float: return "Float";
			case MyEngine::AnimationStateMachineParameterType::Trigger: return "Trigger";
			default: return "Bool";
			}
		}

		static const char* ToStateMachineConditionOperatorString(MyEngine::AnimationStateMachineConditionOperator op)
		{
			switch (op)
			{
			case MyEngine::AnimationStateMachineConditionOperator::IfTrue: return "IfTrue";
			case MyEngine::AnimationStateMachineConditionOperator::IfFalse: return "IfFalse";
			case MyEngine::AnimationStateMachineConditionOperator::Greater: return "Greater";
			case MyEngine::AnimationStateMachineConditionOperator::Less: return "Less";
			case MyEngine::AnimationStateMachineConditionOperator::Trigger: return "Trigger";
			default: return "IfTrue";
			}
		}

		static MyEngine::AnimationStateMachineParameterType ParseStateMachineParameterType(const Value& value)
		{
			if (!value.IsString())
				return MyEngine::AnimationStateMachineParameterType::Bool;

			std::string text = value.GetString();
			if (text == "Float")
				return MyEngine::AnimationStateMachineParameterType::Float;
			if (text == "Trigger")
				return MyEngine::AnimationStateMachineParameterType::Trigger;
			return MyEngine::AnimationStateMachineParameterType::Bool;
		}

		static MyEngine::AnimationStateMachineConditionOperator ParseStateMachineConditionOperator(const Value& value)
		{
			if (!value.IsString())
				return MyEngine::AnimationStateMachineConditionOperator::IfTrue;

			std::string text = value.GetString();
			if (text == "IfFalse")
				return MyEngine::AnimationStateMachineConditionOperator::IfFalse;
			if (text == "Greater")
				return MyEngine::AnimationStateMachineConditionOperator::Greater;
			if (text == "Less")
				return MyEngine::AnimationStateMachineConditionOperator::Less;
			if (text == "Trigger")
				return MyEngine::AnimationStateMachineConditionOperator::Trigger;
			return MyEngine::AnimationStateMachineConditionOperator::IfTrue;
		}

		static void SerializeStateMachineDefinition(PrettyWriter<StringBuffer>& writer, const MyEngine::AnimationStateMachine& sm)
		{
			writer.Key("name"); writer.String(sm.name.c_str());
			writer.Key("defaultStateIndex"); writer.Int(sm.defaultStateIndex);

			writer.Key("parameters");
			writer.StartArray();
			for (const auto& parameter : sm.parameters)
			{
				writer.StartObject();
				writer.Key("name"); writer.String(parameter.name.c_str());
				writer.Key("type"); writer.String(ToStateMachineParameterTypeString(parameter.type));
				writer.Key("defaultFloatValue"); writer.Double(parameter.defaultFloatValue);
				writer.Key("defaultBoolValue"); writer.Bool(parameter.defaultBoolValue);
				writer.EndObject();
			}
			writer.EndArray();

			writer.Key("states");
			writer.StartArray();
			for (const auto& state : sm.states)
			{
				writer.StartObject();
				writer.Key("name"); writer.String(state.name.c_str());
				writer.Key("clipName"); writer.String(state.clipName.c_str());
				writer.Key("loop"); writer.Bool(state.loop);
				writer.Key("playbackSpeed"); writer.Double(std::max(0.01f, state.playbackSpeed));
				writer.Key("trimStartNormalized"); writer.Double(std::clamp(state.trimStartNormalized, 0.0f, 1.0f));
				writer.Key("trimEndNormalized"); writer.Double(std::clamp(state.trimEndNormalized, 0.0f, 1.0f));
				writer.Key("transitions");
				writer.StartArray();
				for (const auto& transition : state.transitions)
				{
					writer.StartObject();
					writer.Key("targetStateIndex"); writer.Int(transition.targetStateIndex);
					writer.Key("blendDuration"); writer.Double(transition.blendDuration);
					writer.Key("requiresExitTime"); writer.Bool(transition.requiresExitTime);
					writer.Key("exitTimeNormalized"); writer.Double(transition.exitTimeNormalized);
					writer.Key("resetTimeOnEnter"); writer.Bool(transition.resetTimeOnEnter);
					writer.Key("conditions");
					writer.StartArray();
					for (const auto& condition : transition.conditions)
					{
						writer.StartObject();
						writer.Key("parameterName"); writer.String(condition.parameterName.c_str());
						writer.Key("op"); writer.String(ToStateMachineConditionOperatorString(condition.op));
						writer.Key("threshold"); writer.Double(condition.threshold);
						writer.EndObject();
					}
					writer.EndArray();
					writer.EndObject();
				}
				writer.EndArray();
				writer.EndObject();
			}
			writer.EndArray();
		}

		static bool DeserializeStateMachineDefinition(const Value& data, MyEngine::AnimationStateMachine& sm)
		{
			if (!data.IsObject())
				return false;

			sm.name = data.HasMember("name") && data["name"].IsString() ? data["name"].GetString() : std::string();
			sm.defaultStateIndex = data.HasMember("defaultStateIndex") && data["defaultStateIndex"].IsInt() ? data["defaultStateIndex"].GetInt() : 0;
			sm.parameters.clear();
			sm.states.clear();

			if (data.HasMember("parameters") && data["parameters"].IsArray())
			{
				for (const auto& paramValue : data["parameters"].GetArray())
				{
					if (!paramValue.IsObject())
						continue;

					MyEngine::AnimationStateMachineParameter param;
					if (paramValue.HasMember("name") && paramValue["name"].IsString())
						param.name = paramValue["name"].GetString();
					if (paramValue.HasMember("type"))
						param.type = ParseStateMachineParameterType(paramValue["type"]);
					if (paramValue.HasMember("defaultFloatValue") && paramValue["defaultFloatValue"].IsNumber())
						param.defaultFloatValue = paramValue["defaultFloatValue"].GetFloat();
					if (paramValue.HasMember("defaultBoolValue") && paramValue["defaultBoolValue"].IsBool())
						param.defaultBoolValue = paramValue["defaultBoolValue"].GetBool();
					sm.parameters.push_back(std::move(param));
				}
			}

			if (data.HasMember("states") && data["states"].IsArray())
			{
				for (const auto& stateValue : data["states"].GetArray())
				{
					if (!stateValue.IsObject())
						continue;

					MyEngine::AnimationStateMachineState state;
					if (stateValue.HasMember("name") && stateValue["name"].IsString())
						state.name = stateValue["name"].GetString();
					if (stateValue.HasMember("clipName") && stateValue["clipName"].IsString())
						state.clipName = stateValue["clipName"].GetString();
					if (stateValue.HasMember("loop") && stateValue["loop"].IsBool())
						state.loop = stateValue["loop"].GetBool();
					if (stateValue.HasMember("playbackSpeed") && stateValue["playbackSpeed"].IsNumber())
						state.playbackSpeed = std::max(0.01f, stateValue["playbackSpeed"].GetFloat());
					if (stateValue.HasMember("trimStartNormalized") && stateValue["trimStartNormalized"].IsNumber())
						state.trimStartNormalized = std::clamp(stateValue["trimStartNormalized"].GetFloat(), 0.0f, 1.0f);
					if (stateValue.HasMember("trimEndNormalized") && stateValue["trimEndNormalized"].IsNumber())
						state.trimEndNormalized = std::clamp(stateValue["trimEndNormalized"].GetFloat(), 0.0f, 1.0f);
					if (state.trimEndNormalized < state.trimStartNormalized)
						std::swap(state.trimStartNormalized, state.trimEndNormalized);
					if (state.trimEndNormalized - state.trimStartNormalized < 0.01f)
					{
						state.trimEndNormalized = std::min(1.0f, state.trimStartNormalized + 0.01f);
						if (state.trimEndNormalized - state.trimStartNormalized < 0.01f)
							state.trimStartNormalized = std::max(0.0f, state.trimEndNormalized - 0.01f);
					}

					if (stateValue.HasMember("transitions") && stateValue["transitions"].IsArray())
					{
						for (const auto& transitionValue : stateValue["transitions"].GetArray())
						{
							if (!transitionValue.IsObject())
								continue;

							MyEngine::AnimationStateMachineTransition transition;
							if (transitionValue.HasMember("targetStateIndex") && transitionValue["targetStateIndex"].IsInt())
								transition.targetStateIndex = transitionValue["targetStateIndex"].GetInt();
							if (transitionValue.HasMember("blendDuration") && transitionValue["blendDuration"].IsNumber())
								transition.blendDuration = transitionValue["blendDuration"].GetFloat();
							if (transitionValue.HasMember("requiresExitTime") && transitionValue["requiresExitTime"].IsBool())
								transition.requiresExitTime = transitionValue["requiresExitTime"].GetBool();
							if (transitionValue.HasMember("exitTimeNormalized") && transitionValue["exitTimeNormalized"].IsNumber())
								transition.exitTimeNormalized = transitionValue["exitTimeNormalized"].GetFloat();
							if (transitionValue.HasMember("resetTimeOnEnter") && transitionValue["resetTimeOnEnter"].IsBool())
								transition.resetTimeOnEnter = transitionValue["resetTimeOnEnter"].GetBool();

							if (transitionValue.HasMember("conditions") && transitionValue["conditions"].IsArray())
							{
								for (const auto& conditionValue : transitionValue["conditions"].GetArray())
								{
									if (!conditionValue.IsObject())
										continue;

									MyEngine::AnimationStateMachineCondition condition;
									if (conditionValue.HasMember("parameterName") && conditionValue["parameterName"].IsString())
										condition.parameterName = conditionValue["parameterName"].GetString();
									if (conditionValue.HasMember("op"))
										condition.op = ParseStateMachineConditionOperator(conditionValue["op"]);
									if (conditionValue.HasMember("threshold") && conditionValue["threshold"].IsNumber())
										condition.threshold = conditionValue["threshold"].GetFloat();
									transition.conditions.push_back(std::move(condition));
								}
							}

							state.transitions.push_back(std::move(transition));
						}
					}

					sm.states.push_back(std::move(state));
				}
			}

			return true;
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
			writer.Key("sunsetSkyboxEnabled"); writer.Bool(scene.sunsetSkyboxEnabled);

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
					writer.Key("pointShadowSizeOverride"); writer.Int(l.pointShadowSizeOverride);
					writer.Key("pointShadowPCFSamplesOverride"); writer.Int(l.pointShadowPCFSamplesOverride);
					writer.Key("pointShadowPCFRadiusOverride"); writer.Double(l.pointShadowPCFRadiusOverride);
					writer.Key("spotShadowSizeOverride"); writer.Int(l.spotShadowSizeOverride);
					writer.Key("spotShadowPCFRadiusOverride"); writer.Double(l.spotShadowPCFRadiusOverride);
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

				if (e->HasComponent<SkeletonComponent>())
				{
					// Skeleton data itself is reconstructed from skinned model assets;
					// persist explicit component presence for completeness/forward compatibility.
					writer.Key("SkeletonComponent");
					writer.StartObject();
					writer.Key("present"); writer.Bool(true);
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
					writer.Key("paintResolution");     writer.Int(tc.paintResolution);
					writer.Key("paintEnabled");        writer.Bool(tc.paintEnabled);
					writer.Key("paintActiveLayer");    writer.Int(tc.paintActiveLayer);
					writer.Key("paintBrushRadius");    writer.Double(tc.paintBrushRadius);
					writer.Key("paintBrushStrength");  writer.Double(tc.paintBrushStrength);
					writer.Key("paintBrushFalloff");   writer.Double(tc.paintBrushFalloff);
					writer.Key("paintLayers");
					writer.StartArray();
					for (const auto& layer : tc.paintLayers)
					{
						writer.StartObject();
						writer.Key("name"); writer.String(layer.name.c_str());
						writer.Key("texturePath"); writer.String(layer.texturePath.c_str());
						writer.Key("uvScale"); writer.Double(layer.uvScale);
						writer.Key("enabled"); writer.Bool(layer.enabled);
						writer.EndObject();
					}
					writer.EndArray();
					writer.Key("paintWeightData");
					writer.StartArray();
					for (float w : tc.paintWeightData)
						writer.Double(w);
					writer.EndArray();
					writer.Key("shaderVertPath");     writer.String(tc.shaderVertPath.c_str());
					writer.Key("shaderFragPath");     writer.String(tc.shaderFragPath.c_str());
					writer.Key("sculptEnabled");      writer.Bool(tc.sculptEnabled);
					writer.Key("sculptBrushRadius");  writer.Double(tc.sculptBrushRadius);
					writer.Key("sculptBrushStrength");writer.Double(tc.sculptBrushStrength);
					writer.Key("sculptBrushFalloff"); writer.Double(tc.sculptBrushFalloff);
					writer.Key("sculptRaise");        writer.Bool(tc.sculptRaise);
					writer.Key("sculptBrushMode");    writer.Int(static_cast<int>(tc.sculptBrushMode));
					writer.Key("sculptFlattenHeight");writer.Double(tc.sculptFlattenHeight);
					writer.EndObject();
				}

				if (e->HasComponent<MovingPlatformComponent>())
				{
					auto& mp = e->GetComponent<MovingPlatformComponent>();
					writer.Key("MovingPlatformComponent");
					writer.StartObject();
					writer.Key("type"); writer.Int(static_cast<int>(mp.type));
					writer.Key("active"); writer.Bool(mp.active);
					writer.Key("pingPong"); writer.Bool(mp.pingPong);
					writer.Key("autoReturn"); writer.Bool(mp.autoReturn);
					writer.Key("startPosition"); SerializeVec3(writer, mp.startPosition);
					writer.Key("endPosition"); SerializeVec3(writer, mp.endPosition);
					writer.Key("speed"); writer.Double(mp.speed);
					writer.Key("waitTime"); writer.Double(mp.waitTime);
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
					writer.Key("enableRootMotion"); writer.Bool(ac.enableRootMotion);
					writer.Key("rootMotionBoneName"); writer.String(ac.rootMotionBoneName.c_str());
					writer.Key("events");
					writer.StartArray();
					for (const auto& evt : ac.events)
					{
						writer.StartObject();
						writer.Key("timeSeconds"); writer.Double(evt.timeSeconds);
						writer.Key("name"); writer.String(evt.name.c_str());
						writer.Key("enabled"); writer.Bool(evt.enabled);
						writer.Key("triggerAudio"); writer.Bool(evt.triggerAudio);
						writer.Key("audioClipPath"); writer.String(evt.audioClipPath.c_str());
						writer.Key("audioVolume"); writer.Double(evt.audioVolume);
						writer.Key("audioPitch"); writer.Double(evt.audioPitch);
						writer.Key("triggerParticleBurst"); writer.Bool(evt.triggerParticleBurst);
						writer.Key("particleBurstCount"); writer.Int(evt.particleBurstCount);
						writer.Key("triggerScriptCallback"); writer.Bool(evt.triggerScriptCallback);
						writer.Key("scriptCallbackName"); writer.String(evt.scriptCallbackName.c_str());
						writer.EndObject();
					}
					writer.EndArray();
					writer.Key("importedAnimationFiles");
					writer.StartArray();
					for (const auto& filePath : ac.importedAnimationFilePaths)
						writer.String(filePath.c_str());
					writer.EndArray();
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
					if (sm.stateMachine)
					{
						writer.Key("stateMachineData");
						writer.StartObject();
						SerializeStateMachineDefinition(writer, *sm.stateMachine);
						writer.EndObject();
					}
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
					writer.Key("isVariantInstance"); writer.Bool(prefab.isVariantInstance);
					writer.Key("variantBasePrefabPath"); writer.String(prefab.variantBasePrefabPath.c_str());
					writer.Key("variantBaseEntityID"); writer.Uint(prefab.variantBaseEntityID);
					writer.Key("overrideName"); writer.Bool(prefab.overrideName);
					writer.Key("overrideTag"); writer.Bool(prefab.overrideTag);
					writer.Key("overrideLayer"); writer.Bool(prefab.overrideLayer);
					writer.Key("overrideTransform"); writer.Bool(prefab.overrideTransform);
					writer.Key("overrideMeshRenderer"); writer.Bool(prefab.overrideMeshRenderer);
					writer.Key("overrideLight"); writer.Bool(prefab.overrideLight);
					writer.Key("overrideRigidbody"); writer.Bool(prefab.overrideRigidbody);
					writer.Key("overrideScript"); writer.Bool(prefab.overrideScript);
					writer.Key("overrideAnimation"); writer.Bool(prefab.overrideAnimation);
					writer.Key("overrideAudioSource"); writer.Bool(prefab.overrideAudioSource);
					writer.Key("overrideAudioListener"); writer.Bool(prefab.overrideAudioListener);
					writer.Key("overrideBoxCollider"); writer.Bool(prefab.overrideBoxCollider);
					writer.Key("overrideCapsuleCollider"); writer.Bool(prefab.overrideCapsuleCollider);
					writer.Key("overridePlaneCollider"); writer.Bool(prefab.overridePlaneCollider);
					writer.Key("overrideBoundingSphere"); writer.Bool(prefab.overrideBoundingSphere);
					writer.Key("overrideMeshCollider"); writer.Bool(prefab.overrideMeshCollider);
					writer.Key("overrideCharacterController"); writer.Bool(prefab.overrideCharacterController);
					writer.Key("overrideNavigationAgent"); writer.Bool(prefab.overrideNavigationAgent);
					writer.Key("overrideTerrain"); writer.Bool(prefab.overrideTerrain);
					writer.Key("overrideParticleEmitter"); writer.Bool(prefab.overrideParticleEmitter);
					writer.Key("overrideLOD"); writer.Bool(prefab.overrideLOD);
					writer.Key("overrideCollisionEvents"); writer.Bool(prefab.overrideCollisionEvents);
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
					writer.Key("enableSprintSlide"); writer.Bool(controller.enableSprintSlide);
					writer.Key("sprintMultiplier"); writer.Double(controller.sprintMultiplier);
					writer.Key("slideSpeedMultiplier"); writer.Double(controller.slideSpeedMultiplier);
					writer.Key("slideDuration"); writer.Double(controller.slideDuration);
					writer.Key("slideCooldown"); writer.Double(controller.slideCooldown);
					writer.Key("turnSpeed"); writer.Double(controller.turnSpeed);
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

				if (e->HasComponent<CombatAttackComponent>())
				{
					auto& combatAttack = e->GetComponent<CombatAttackComponent>();
					writer.Key("CombatAttack");
					writer.StartObject();
					writer.Key("attackSetPath"); writer.String(combatAttack.attackSetPath.c_str());
					writer.Key("autoLoadFromJson"); writer.Bool(combatAttack.autoLoadFromJson);
					writer.Key("selectedAttackIndex"); writer.Int(combatAttack.selectedAttackIndex);
					writer.Key("attacks");
					writer.StartArray();
					for (const auto& attackDef : combatAttack.attacks)
					{
						writer.StartObject();
						writer.Key("name"); writer.String(attackDef.name.c_str());
						writer.Key("startupFrames"); writer.Int(attackDef.startupFrames);
						writer.Key("activeFrames"); writer.Int(attackDef.activeFrames);
						writer.Key("recoveryFrames"); writer.Int(attackDef.recoveryFrames);
						writer.Key("damage"); writer.Double(attackDef.damage);
						writer.Key("postureDamage"); writer.Double(attackDef.postureDamage);
						writer.Key("hitboxCenter"); SerializeVec3(writer, attackDef.hitboxCenter);
						writer.Key("hitboxRadius"); writer.Double(attackDef.hitboxRadius);
						writer.Key("cooldownSeconds"); writer.Double(attackDef.cooldownSeconds);
						writer.EndObject();
					}
					writer.EndArray();
					writer.EndObject();
				}

				if (e->HasComponent<CombatStatsComponent>())
				{
					auto& combatStats = e->GetComponent<CombatStatsComponent>();
					writer.Key("CombatStats");
					writer.StartObject();
					writer.Key("maxHealth"); writer.Double(combatStats.maxHealth);
					writer.Key("health"); writer.Double(combatStats.health);
					writer.Key("maxPosture"); writer.Double(combatStats.maxPosture);
					writer.Key("posture"); writer.Double(combatStats.posture);
					writer.Key("postureRecoveryPerSecond"); writer.Double(combatStats.postureRecoveryPerSecond);
					writer.Key("guardPostureMultiplier"); writer.Double(combatStats.guardPostureMultiplier);
					writer.EndObject();
				}

				if (e->HasComponent<BossAIComponent>())
				{
					auto& bossAI = e->GetComponent<BossAIComponent>();
					writer.Key("BossAI");
					writer.StartObject();
					writer.Key("enabled"); writer.Bool(bossAI.enabled);
					writer.Key("targetEntityID"); writer.Uint(bossAI.targetEntityID);
					writer.Key("approachSpeed"); writer.Double(bossAI.approachSpeed);
					writer.Key("strafeSpeed"); writer.Double(bossAI.strafeSpeed);
					writer.Key("desiredRange"); writer.Double(bossAI.desiredRange);
					writer.Key("attackRange"); writer.Double(bossAI.attackRange);
					writer.Key("punishRange"); writer.Double(bossAI.punishRange);
					writer.Key("decisionInterval"); writer.Double(bossAI.decisionInterval);
					writer.Key("punishWindowSeconds"); writer.Double(bossAI.punishWindowSeconds);
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
					writer.Key("busName"); writer.String(as.busName.c_str());
					writer.Key("eventName"); writer.String(as.eventName.c_str());
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

				if (e->HasComponent<CollisionEventsComponent>())
				{
					// Runtime callbacks are not serializable; persist component presence.
					writer.Key("CollisionEvents");
					writer.StartObject();
					writer.Key("enabled"); writer.Bool(true);
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
			if (json.empty())
				return false;
			return LoadScene(scene, json, defaultShader, outGlobalScripts);
		}

		bool LoadScene(
			::Scene& scene,
			const std::string& path,
			const std::shared_ptr<MyEngine::Shader>& defaultShader,
			std::vector<MyEngine::ScriptSystem::GlobalScriptConfig>* outGlobalScripts
		)
		{
			std::string content;
			bool loadedFromRawJson = false;

			std::ifstream ifs(path);
			if (ifs)
			{
				content.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
				ifs.close();
			}
			else
			{
				size_t firstNonWhitespace = path.find_first_not_of(" \t\n\r");
				const bool looksLikeJson =
					(firstNonWhitespace != std::string::npos) &&
					(path[firstNonWhitespace] == '{' || path[firstNonWhitespace] == '[');
				if (!looksLikeJson)
				{
					std::cerr << "Failed to open " << path << " for reading." << std::endl;
					return false;
				}
				content = path;
				loadedFromRawJson = true;
			}

			Document doc;
			if (doc.Parse(content.c_str()).HasParseError())
			{
				if (loadedFromRawJson)
					std::cerr << "Failed to parse scene JSON from string." << std::endl;
				else
					std::cerr << "Failed to parse scene JSON: " << path << std::endl;
				return false;
			}

			if (!doc.HasMember("entities") || !doc["entities"].IsArray())
				return false;

			// Full scene load replaces current scene content.
			scene.Clear();
			if (doc.HasMember("sunsetSkyboxEnabled") && doc["sunsetSkyboxEnabled"].IsBool())
				scene.sunsetSkyboxEnabled = doc["sunsetSkyboxEnabled"].GetBool();

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
					if (lo.HasMember("pointShadowSizeOverride") && lo["pointShadowSizeOverride"].IsInt()) l.pointShadowSizeOverride = lo["pointShadowSizeOverride"].GetInt();
					if (lo.HasMember("pointShadowPCFSamplesOverride") && lo["pointShadowPCFSamplesOverride"].IsInt()) l.pointShadowPCFSamplesOverride = lo["pointShadowPCFSamplesOverride"].GetInt();
					if (lo.HasMember("pointShadowPCFRadiusOverride") && lo["pointShadowPCFRadiusOverride"].IsNumber()) l.pointShadowPCFRadiusOverride = static_cast<float>(lo["pointShadowPCFRadiusOverride"].GetDouble());
					if (lo.HasMember("spotShadowSizeOverride") && lo["spotShadowSizeOverride"].IsInt()) l.spotShadowSizeOverride = lo["spotShadowSizeOverride"].GetInt();
					if (lo.HasMember("spotShadowPCFRadiusOverride") && lo["spotShadowPCFRadiusOverride"].IsNumber()) l.spotShadowPCFRadiusOverride = static_cast<float>(lo["spotShadowPCFRadiusOverride"].GetDouble());
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
					if (to.HasMember("paintResolution") && to["paintResolution"].IsInt()) tc.paintResolution = std::clamp(to["paintResolution"].GetInt(), 2, 2048);
					if (to.HasMember("paintEnabled")) tc.paintEnabled = to["paintEnabled"].GetBool();
					if (to.HasMember("paintActiveLayer") && to["paintActiveLayer"].IsInt()) tc.paintActiveLayer = std::clamp(to["paintActiveLayer"].GetInt(), 0, kMaxTerrainPaintLayers - 1);
					if (to.HasMember("paintBrushRadius")) tc.paintBrushRadius = static_cast<float>(to["paintBrushRadius"].GetDouble());
					if (to.HasMember("paintBrushStrength")) tc.paintBrushStrength = static_cast<float>(to["paintBrushStrength"].GetDouble());
					if (to.HasMember("paintBrushFalloff")) tc.paintBrushFalloff = static_cast<float>(to["paintBrushFalloff"].GetDouble());
					if (to.HasMember("paintLayers") && to["paintLayers"].IsArray())
					{
						tc.paintLayers.clear();
						for (const auto& lv : to["paintLayers"].GetArray())
						{
							if (!lv.IsObject()) continue;
							TerrainPaintLayer layer;
							if (lv.HasMember("name") && lv["name"].IsString()) layer.name = lv["name"].GetString();
							if (lv.HasMember("texturePath") && lv["texturePath"].IsString())
							{
								layer.texturePath = lv["texturePath"].GetString();
								if (!layer.texturePath.empty())
									layer.texture = MyEngine::AssetManager::LoadTexture(layer.texturePath);
							}
							if (lv.HasMember("uvScale")) layer.uvScale = static_cast<float>(lv["uvScale"].GetDouble());
							if (lv.HasMember("enabled")) layer.enabled = lv["enabled"].GetBool();
							tc.paintLayers.push_back(std::move(layer));
							if (tc.paintLayers.size() >= static_cast<size_t>(kMaxTerrainPaintLayers))
								break;
						}
					}
					if (to.HasMember("paintWeightData") && to["paintWeightData"].IsArray())
					{
						tc.paintWeightData.clear();
						tc.paintWeightData.reserve(to["paintWeightData"].Size());
						for (const auto& wv : to["paintWeightData"].GetArray())
						{
							if (wv.IsNumber())
								tc.paintWeightData.push_back(static_cast<float>(wv.GetDouble()));
						}
						tc.paintWeightTextureDirty = true;
					}
					if (to.HasMember("shaderVertPath"))     tc.shaderVertPath     = to["shaderVertPath"].GetString();
					if (to.HasMember("shaderFragPath"))     tc.shaderFragPath     = to["shaderFragPath"].GetString();
					if (to.HasMember("sculptEnabled"))      tc.sculptEnabled      = to["sculptEnabled"].GetBool();
					if (to.HasMember("sculptBrushRadius"))  tc.sculptBrushRadius  = static_cast<float>(to["sculptBrushRadius"].GetDouble());
					if (to.HasMember("sculptBrushStrength"))tc.sculptBrushStrength= static_cast<float>(to["sculptBrushStrength"].GetDouble());
					if (to.HasMember("sculptBrushFalloff")) tc.sculptBrushFalloff = static_cast<float>(to["sculptBrushFalloff"].GetDouble());
					if (to.HasMember("sculptRaise"))        tc.sculptRaise        = to["sculptRaise"].GetBool();
					if (to.HasMember("sculptBrushMode") && to["sculptBrushMode"].IsInt())
					{
						const int brushMode = std::clamp(to["sculptBrushMode"].GetInt(), 0, 2);
						tc.sculptBrushMode = static_cast<TerrainBrushMode>(brushMode);
					}
					if (to.HasMember("sculptFlattenHeight")) tc.sculptFlattenHeight = static_cast<float>(to["sculptFlattenHeight"].GetDouble());
					if (!tc.surfaceTexturePath.empty())
						tc.surfaceTexture = MyEngine::AssetManager::LoadTexture(tc.surfaceTexturePath);
					if (!tc.shaderVertPath.empty() && !tc.shaderFragPath.empty())
						tc.shader = MyEngine::AssetManager::LoadShader(tc.shaderVertPath, tc.shaderFragPath);
					tc.dirty = true;
				}

				if (v.HasMember("MovingPlatformComponent") && v["MovingPlatformComponent"].IsObject())
				{
					const auto& mo = v["MovingPlatformComponent"];
					auto& mp = ent->AddComponent<MovingPlatformComponent>();
					if (mo.HasMember("type")) mp.type = static_cast<MyEngine::MovingPartType>(mo["type"].GetInt());
					if (mo.HasMember("active")) mp.active = mo["active"].GetBool();
					if (mo.HasMember("pingPong")) mp.pingPong = mo["pingPong"].GetBool();
					if (mo.HasMember("autoReturn")) mp.autoReturn = mo["autoReturn"].GetBool();
					if (mo.HasMember("startPosition")) mp.startPosition = DeserializeVec3(mo["startPosition"]);
					if (mo.HasMember("endPosition")) mp.endPosition = DeserializeVec3(mo["endPosition"]);
					if (mo.HasMember("speed")) mp.speed = static_cast<float>(mo["speed"].GetDouble());
					if (mo.HasMember("waitTime")) mp.waitTime = static_cast<float>(mo["waitTime"].GetDouble());
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

				if (v.HasMember("SkeletonComponent") && v["SkeletonComponent"].IsObject())
				{
					if (!ent->HasComponent<SkeletonComponent>())
						ent->AddComponent<SkeletonComponent>();
				}

				// AnimationComponent playback state. If the entity did not get an
				// animation component from skinned-model reload, create one so
				// serialized event tracks/playback flags still roundtrip.
				if (v.HasMember("AnimationComponent") && v["AnimationComponent"].IsObject())
				{
					auto& ac = ent->HasComponent<AnimationComponent>()
						? ent->GetComponent<AnimationComponent>()
						: ent->AddComponent<AnimationComponent>();
					const auto& aco = v["AnimationComponent"];
					if (aco.HasMember("activeClipIndex")) ac.activeClipIndex = aco["activeClipIndex"].GetInt();
					if (aco.HasMember("time")) ac.time = static_cast<float>(aco["time"].GetDouble());
					if (aco.HasMember("playbackSpeed")) ac.playbackSpeed = static_cast<float>(aco["playbackSpeed"].GetDouble());
					if (aco.HasMember("playing")) ac.playing = aco["playing"].GetBool();
					if (aco.HasMember("looping")) ac.looping = aco["looping"].GetBool();
					if (aco.HasMember("enableRootMotion")) ac.enableRootMotion = aco["enableRootMotion"].GetBool();
					if (aco.HasMember("rootMotionBoneName") && aco["rootMotionBoneName"].IsString()) ac.rootMotionBoneName = aco["rootMotionBoneName"].GetString();
					if (aco.HasMember("events") && aco["events"].IsArray())
					{
						ac.events.clear();
						for (const auto& evtValue : aco["events"].GetArray())
						{
							if (!evtValue.IsObject())
								continue;
							AnimationComponent::AnimationEvent evt;
							if (evtValue.HasMember("timeSeconds") && evtValue["timeSeconds"].IsNumber())
								evt.timeSeconds = static_cast<float>(evtValue["timeSeconds"].GetDouble());
							if (evtValue.HasMember("name") && evtValue["name"].IsString())
								evt.name = evtValue["name"].GetString();
							if (evtValue.HasMember("enabled") && evtValue["enabled"].IsBool())
								evt.enabled = evtValue["enabled"].GetBool();
							if (evtValue.HasMember("triggerAudio") && evtValue["triggerAudio"].IsBool())
								evt.triggerAudio = evtValue["triggerAudio"].GetBool();
							if (evtValue.HasMember("audioClipPath") && evtValue["audioClipPath"].IsString())
								evt.audioClipPath = evtValue["audioClipPath"].GetString();
							if (evtValue.HasMember("audioVolume") && evtValue["audioVolume"].IsNumber())
								evt.audioVolume = static_cast<float>(evtValue["audioVolume"].GetDouble());
							if (evtValue.HasMember("audioPitch") && evtValue["audioPitch"].IsNumber())
								evt.audioPitch = static_cast<float>(evtValue["audioPitch"].GetDouble());
							if (evtValue.HasMember("triggerParticleBurst") && evtValue["triggerParticleBurst"].IsBool())
								evt.triggerParticleBurst = evtValue["triggerParticleBurst"].GetBool();
							if (evtValue.HasMember("particleBurstCount") && evtValue["particleBurstCount"].IsInt())
								evt.particleBurstCount = evtValue["particleBurstCount"].GetInt();
							if (evtValue.HasMember("triggerScriptCallback") && evtValue["triggerScriptCallback"].IsBool())
								evt.triggerScriptCallback = evtValue["triggerScriptCallback"].GetBool();
							if (evtValue.HasMember("scriptCallbackName") && evtValue["scriptCallbackName"].IsString())
								evt.scriptCallbackName = evtValue["scriptCallbackName"].GetString();
							ac.events.push_back(std::move(evt));
						}
					}

					ac.importedAnimationFilePaths.clear();
					if (aco.HasMember("importedAnimationFiles") && aco["importedAnimationFiles"].IsArray())
					{
						for (const auto& fileValue : aco["importedAnimationFiles"].GetArray())
						{
							if (!fileValue.IsString())
								continue;
							ac.importedAnimationFilePaths.push_back(fileValue.GetString());
						}
					}

					std::shared_ptr<MyEngine::Skeleton> skeleton;
					if (ent->HasComponent<SkeletonComponent>())
						skeleton = ent->GetComponent<SkeletonComponent>().skeleton;

					if (!ac.importedAnimationFilePaths.empty())
					{
						if (!ac.clips)
							ac.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();

						for (const auto& importedPath : ac.importedAnimationFilePaths)
						{
							auto externalClips = MyEngine::AssetManager::LoadAnimationClips(importedPath);
							if (!externalClips || externalClips->empty())
								continue;

							std::filesystem::path sourcePath(importedPath);
							std::string sourceStem = sourcePath.stem().string();

							auto isGenericImportedName = [](const std::string& name)
							{
								if (name.empty())
									return true;

								std::string trimmed = name;
								trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
								trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

								std::string lower = trimmed;
								std::transform(lower.begin(), lower.end(), lower.begin(),
									[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

								return lower.empty() ||
									lower == "mixamo.com" ||
									lower == "mixamo_com" ||
									lower == "mixamo.com|mixamo.com";
							};

							for (const auto& clip : *externalClips)
							{
								if (skeleton && !MyEngine::AssetManager::IsAnimationClipCompatible(clip, skeleton))
									continue;

								MyEngine::AnimationClip importedClip = clip;
								std::string baseName = isGenericImportedName(importedClip.name) ? sourceStem : importedClip.name;
								importedClip.name = baseName;

								bool duplicateName = false;
								for (const auto& existingClip : *ac.clips)
								{
									if (existingClip.name == importedClip.name)
									{
										duplicateName = true;
										break;
									}
								}
								if (duplicateName)
									importedClip.name += " [" + sourceStem + "]";

								ac.clips->push_back(std::move(importedClip));
							}
						}
					}

					if (ac.clips && !ac.clips->empty())
					{
						ac.activeClipIndex = std::clamp(ac.activeClipIndex, 0, static_cast<int>(ac.clips->size()) - 1);
					}
					else
					{
						ac.activeClipIndex = 0;
						ac.time = 0.0f;
					}
				}

				if (v.HasMember("AnimationStateMachineComponent") && v["AnimationStateMachineComponent"].IsObject())
				{
					auto& sm = ent->AddComponent<AnimationStateMachineComponent>();
					const auto& smo = v["AnimationStateMachineComponent"];
					if (smo.HasMember("assetPath") && smo["assetPath"].IsString())
						sm.assetPath = smo["assetPath"].GetString();

					// Prefer scene-embedded state machine data so manual scene saves
					// reliably restore animation timing/trim/speed edits on engine reload.
					bool loadedFromSceneData = false;
					if (smo.HasMember("stateMachineData") && smo["stateMachineData"].IsObject())
					{
						sm.stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
						if (DeserializeStateMachineDefinition(smo["stateMachineData"], *sm.stateMachine))
						{
							loadedFromSceneData = true;
						}
						else
						{
							sm.stateMachine.reset();
						}
					}

					if (!loadedFromSceneData && !sm.assetPath.empty())
					{
						sm.stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
						if (!sm.stateMachine->LoadFromFile(sm.assetPath))
						{
							sm.stateMachine.reset();
							sm.assetPath.clear();
						}
					}
					if (smo.HasMember("currentStateIndex")) sm.currentStateIndex = smo["currentStateIndex"].GetInt();
					if (smo.HasMember("pendingStateIndex")) sm.pendingStateIndex = smo["pendingStateIndex"].GetInt();
					if (smo.HasMember("currentStateTime")) sm.currentStateTime = static_cast<float>(smo["currentStateTime"].GetDouble());
					if (smo.HasMember("autoInitialize")) sm.autoInitialize = smo["autoInitialize"].GetBool();
					if (smo.HasMember("debugPauseTransitions")) sm.debugPauseTransitions = smo["debugPauseTransitions"].GetBool();
					if (sm.autoInitialize)
						sm.ResetRuntimeState();
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
					if (po.HasMember("isVariantInstance") && po["isVariantInstance"].IsBool())
						prefab.isVariantInstance = po["isVariantInstance"].GetBool();
					if (po.HasMember("variantBasePrefabPath") && po["variantBasePrefabPath"].IsString())
						prefab.variantBasePrefabPath = po["variantBasePrefabPath"].GetString();
					if (po.HasMember("variantBaseEntityID") && po["variantBaseEntityID"].IsUint())
						prefab.variantBaseEntityID = po["variantBaseEntityID"].GetUint();
					if (po.HasMember("overrideName") && po["overrideName"].IsBool())
						prefab.overrideName = po["overrideName"].GetBool();
					if (po.HasMember("overrideTag") && po["overrideTag"].IsBool())
						prefab.overrideTag = po["overrideTag"].GetBool();
					if (po.HasMember("overrideLayer") && po["overrideLayer"].IsBool())
						prefab.overrideLayer = po["overrideLayer"].GetBool();
					if (po.HasMember("overrideTransform") && po["overrideTransform"].IsBool())
						prefab.overrideTransform = po["overrideTransform"].GetBool();
					if (po.HasMember("overrideMeshRenderer") && po["overrideMeshRenderer"].IsBool())
						prefab.overrideMeshRenderer = po["overrideMeshRenderer"].GetBool();
					if (po.HasMember("overrideLight") && po["overrideLight"].IsBool())
						prefab.overrideLight = po["overrideLight"].GetBool();
					if (po.HasMember("overrideRigidbody") && po["overrideRigidbody"].IsBool())
						prefab.overrideRigidbody = po["overrideRigidbody"].GetBool();
					if (po.HasMember("overrideScript") && po["overrideScript"].IsBool())
						prefab.overrideScript = po["overrideScript"].GetBool();
					if (po.HasMember("overrideAnimation") && po["overrideAnimation"].IsBool())
						prefab.overrideAnimation = po["overrideAnimation"].GetBool();
					if (po.HasMember("overrideAudioSource") && po["overrideAudioSource"].IsBool())
						prefab.overrideAudioSource = po["overrideAudioSource"].GetBool();
					if (po.HasMember("overrideAudioListener") && po["overrideAudioListener"].IsBool())
						prefab.overrideAudioListener = po["overrideAudioListener"].GetBool();
					if (po.HasMember("overrideBoxCollider") && po["overrideBoxCollider"].IsBool())
						prefab.overrideBoxCollider = po["overrideBoxCollider"].GetBool();
					if (po.HasMember("overrideCapsuleCollider") && po["overrideCapsuleCollider"].IsBool())
						prefab.overrideCapsuleCollider = po["overrideCapsuleCollider"].GetBool();
					if (po.HasMember("overridePlaneCollider") && po["overridePlaneCollider"].IsBool())
						prefab.overridePlaneCollider = po["overridePlaneCollider"].GetBool();
					if (po.HasMember("overrideBoundingSphere") && po["overrideBoundingSphere"].IsBool())
						prefab.overrideBoundingSphere = po["overrideBoundingSphere"].GetBool();
					if (po.HasMember("overrideMeshCollider") && po["overrideMeshCollider"].IsBool())
						prefab.overrideMeshCollider = po["overrideMeshCollider"].GetBool();
					if (po.HasMember("overrideCharacterController") && po["overrideCharacterController"].IsBool())
						prefab.overrideCharacterController = po["overrideCharacterController"].GetBool();
					if (po.HasMember("overrideNavigationAgent") && po["overrideNavigationAgent"].IsBool())
						prefab.overrideNavigationAgent = po["overrideNavigationAgent"].GetBool();
					if (po.HasMember("overrideTerrain") && po["overrideTerrain"].IsBool())
						prefab.overrideTerrain = po["overrideTerrain"].GetBool();
					if (po.HasMember("overrideParticleEmitter") && po["overrideParticleEmitter"].IsBool())
						prefab.overrideParticleEmitter = po["overrideParticleEmitter"].GetBool();
					if (po.HasMember("overrideLOD") && po["overrideLOD"].IsBool())
						prefab.overrideLOD = po["overrideLOD"].GetBool();
					if (po.HasMember("overrideCollisionEvents") && po["overrideCollisionEvents"].IsBool())
						prefab.overrideCollisionEvents = po["overrideCollisionEvents"].GetBool();
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
					if (cco.HasMember("enableSprintSlide")) controller.enableSprintSlide = cco["enableSprintSlide"].GetBool();
					if (cco.HasMember("sprintMultiplier")) controller.sprintMultiplier = static_cast<float>(cco["sprintMultiplier"].GetDouble());
					if (cco.HasMember("slideSpeedMultiplier")) controller.slideSpeedMultiplier = static_cast<float>(cco["slideSpeedMultiplier"].GetDouble());
					if (cco.HasMember("slideDuration")) controller.slideDuration = static_cast<float>(cco["slideDuration"].GetDouble());
					if (cco.HasMember("slideCooldown")) controller.slideCooldown = static_cast<float>(cco["slideCooldown"].GetDouble());
					if (cco.HasMember("turnSpeed")) controller.turnSpeed = static_cast<float>(cco["turnSpeed"].GetDouble());
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

				if (v.HasMember("CombatAttack") && v["CombatAttack"].IsObject())
				{
					auto& combatAttack = ent->AddComponent<CombatAttackComponent>();
					const auto& cao = v["CombatAttack"];
					if (cao.HasMember("attackSetPath") && cao["attackSetPath"].IsString()) combatAttack.attackSetPath = cao["attackSetPath"].GetString();
					if (cao.HasMember("autoLoadFromJson")) combatAttack.autoLoadFromJson = cao["autoLoadFromJson"].GetBool();
					if (cao.HasMember("selectedAttackIndex")) combatAttack.selectedAttackIndex = cao["selectedAttackIndex"].GetInt();
					if (cao.HasMember("attacks") && cao["attacks"].IsArray())
					{
						combatAttack.attacks.clear();
						for (const auto& attackValue : cao["attacks"].GetArray())
						{
							if (!attackValue.IsObject())
								continue;
							CombatAttackDefinition def;
							if (attackValue.HasMember("name") && attackValue["name"].IsString()) def.name = attackValue["name"].GetString();
							if (attackValue.HasMember("startupFrames")) def.startupFrames = attackValue["startupFrames"].GetInt();
							if (attackValue.HasMember("activeFrames")) def.activeFrames = attackValue["activeFrames"].GetInt();
							if (attackValue.HasMember("recoveryFrames")) def.recoveryFrames = attackValue["recoveryFrames"].GetInt();
							if (attackValue.HasMember("damage")) def.damage = static_cast<float>(attackValue["damage"].GetDouble());
							if (attackValue.HasMember("postureDamage")) def.postureDamage = static_cast<float>(attackValue["postureDamage"].GetDouble());
							if (attackValue.HasMember("hitboxCenter")) def.hitboxCenter = DeserializeVec3(attackValue["hitboxCenter"]);
							if (attackValue.HasMember("hitboxRadius")) def.hitboxRadius = static_cast<float>(attackValue["hitboxRadius"].GetDouble());
							if (attackValue.HasMember("cooldownSeconds")) def.cooldownSeconds = static_cast<float>(attackValue["cooldownSeconds"].GetDouble());
							combatAttack.attacks.push_back(def);
						}
					}
				}

				if (v.HasMember("CombatStats") && v["CombatStats"].IsObject())
				{
					auto& combatStats = ent->AddComponent<CombatStatsComponent>();
					const auto& cso = v["CombatStats"];
					if (cso.HasMember("maxHealth")) combatStats.maxHealth = static_cast<float>(cso["maxHealth"].GetDouble());
					if (cso.HasMember("health")) combatStats.health = static_cast<float>(cso["health"].GetDouble());
					if (cso.HasMember("maxPosture")) combatStats.maxPosture = static_cast<float>(cso["maxPosture"].GetDouble());
					if (cso.HasMember("posture")) combatStats.posture = static_cast<float>(cso["posture"].GetDouble());
					if (cso.HasMember("postureRecoveryPerSecond")) combatStats.postureRecoveryPerSecond = static_cast<float>(cso["postureRecoveryPerSecond"].GetDouble());
					if (cso.HasMember("guardPostureMultiplier")) combatStats.guardPostureMultiplier = static_cast<float>(cso["guardPostureMultiplier"].GetDouble());
				}

				if (v.HasMember("BossAI") && v["BossAI"].IsObject())
				{
					auto& bossAI = ent->AddComponent<BossAIComponent>();
					const auto& bio = v["BossAI"];
					if (bio.HasMember("enabled")) bossAI.enabled = bio["enabled"].GetBool();
					if (bio.HasMember("targetEntityID")) bossAI.targetEntityID = bio["targetEntityID"].GetUint();
					if (bio.HasMember("approachSpeed")) bossAI.approachSpeed = static_cast<float>(bio["approachSpeed"].GetDouble());
					if (bio.HasMember("strafeSpeed")) bossAI.strafeSpeed = static_cast<float>(bio["strafeSpeed"].GetDouble());
					if (bio.HasMember("desiredRange")) bossAI.desiredRange = static_cast<float>(bio["desiredRange"].GetDouble());
					if (bio.HasMember("attackRange")) bossAI.attackRange = static_cast<float>(bio["attackRange"].GetDouble());
					if (bio.HasMember("punishRange")) bossAI.punishRange = static_cast<float>(bio["punishRange"].GetDouble());
					if (bio.HasMember("decisionInterval")) bossAI.decisionInterval = static_cast<float>(bio["decisionInterval"].GetDouble());
					if (bio.HasMember("punishWindowSeconds")) bossAI.punishWindowSeconds = static_cast<float>(bio["punishWindowSeconds"].GetDouble());
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
					if (ao.HasMember("busName") && ao["busName"].IsString()) as.busName = ao["busName"].GetString();
					if (ao.HasMember("eventName") && ao["eventName"].IsString()) as.eventName = ao["eventName"].GetString();
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

				if (v.HasMember("CollisionEvents") && v["CollisionEvents"].IsObject())
				{
					if (!ent->HasComponent<CollisionEventsComponent>())
						ent->AddComponent<CollisionEventsComponent>();
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
									COPY_COMPONENT(CombatAttackComponent);
									COPY_COMPONENT(CombatStatsComponent);
									COPY_COMPONENT(BossAIComponent);
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
									COPY_COMPONENT(MovingPlatformComponent);
									COPY_COMPONENT(NavigationAgentComponent);
									COPY_COMPONENT(ParticleEmitterComponent);
									COPY_COMPONENT(CollisionEventsComponent);
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

								bool SavePrefabVariant(
									const ::Scene& sourceScene,
									::Entity* rootEntity,
									const std::string& variantPath,
									const std::string& basePrefabPath,
									std::uint32_t baseEntityID
								)
								{
									if (!rootEntity || variantPath.empty() || basePrefabPath.empty() || baseEntityID == 0)
										return false;

									if (!SavePrefab(sourceScene, rootEntity, variantPath))
										return false;

									::Scene variantScene;
									if (!LoadScene(variantScene, variantPath, nullptr))
										return false;

									auto root = variantScene.GetEntityByID(rootEntity->GetID());
									if (!root)
									{
										auto& entities = variantScene.GetEntities();
										if (entities.empty() || !entities.front())
											return false;
										root = entities.front().get();
									}

									auto& prefabMeta = root->HasComponent<PrefabInstanceComponent>()
										? root->GetComponent<PrefabInstanceComponent>()
										: root->AddComponent<PrefabInstanceComponent>();
									prefabMeta.sourcePrefabPath = basePrefabPath;
									prefabMeta.sourceEntityID = baseEntityID;
									prefabMeta.isVariantInstance = true;
									prefabMeta.variantBasePrefabPath = basePrefabPath;
									prefabMeta.variantBaseEntityID = baseEntityID;
									prefabMeta.overrideName = false;
									prefabMeta.overrideTag = false;
									prefabMeta.overrideLayer = false;
									prefabMeta.overrideTransform = false;
									prefabMeta.overrideMeshRenderer = false;
									prefabMeta.overrideLight = false;
									prefabMeta.overrideRigidbody = false;
									prefabMeta.overrideScript = false;
									prefabMeta.overrideAnimation = false;
									prefabMeta.overrideAudioSource = false;
									prefabMeta.overrideAudioListener = false;
									prefabMeta.overrideBoxCollider = false;
									prefabMeta.overrideCapsuleCollider = false;
									prefabMeta.overridePlaneCollider = false;
									prefabMeta.overrideBoundingSphere = false;
									prefabMeta.overrideMeshCollider = false;
									prefabMeta.overrideCharacterController = false;
									prefabMeta.overrideNavigationAgent = false;
									prefabMeta.overrideTerrain = false;
									prefabMeta.overrideParticleEmitter = false;
									prefabMeta.overrideLOD = false;
									prefabMeta.overrideCollisionEvents = false;

									::Scene baseScene;
									if (LoadScene(baseScene, basePrefabPath, nullptr))
									{
										auto baseRoot = baseScene.GetEntityByID(baseEntityID);
										if (baseRoot)
										{
											auto nearlyEqual = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
											auto vec3Equal = [&](const glm::vec3& a, const glm::vec3& b)
											{
												return nearlyEqual(a.x, b.x) && nearlyEqual(a.y, b.y) && nearlyEqual(a.z, b.z);
											};

											prefabMeta.overrideName = (root->GetName() != baseRoot->GetName());
											prefabMeta.overrideTag = (root->GetTag() != baseRoot->GetTag());
											prefabMeta.overrideLayer = (root->GetLayer() != baseRoot->GetLayer());

											if (root->HasComponent<TransformComponent>() != baseRoot->HasComponent<TransformComponent>())
												prefabMeta.overrideTransform = true;
											else if (root->HasComponent<TransformComponent>())
											{
												const auto& a = root->GetComponent<TransformComponent>();
												const auto& b = baseRoot->GetComponent<TransformComponent>();
												prefabMeta.overrideTransform = !vec3Equal(a.position, b.position) || !vec3Equal(a.rotation, b.rotation) || !vec3Equal(a.scale, b.scale);
											}

											if (root->HasComponent<MeshRendererComponent>() != baseRoot->HasComponent<MeshRendererComponent>())
												prefabMeta.overrideMeshRenderer = true;
											else if (root->HasComponent<MeshRendererComponent>())
											{
												const auto& a = root->GetComponent<MeshRendererComponent>();
												const auto& b = baseRoot->GetComponent<MeshRendererComponent>();
												prefabMeta.overrideMeshRenderer = (a.visible != b.visible) || (a.materialPath != b.materialPath) || (a.useTexture != b.useTexture) || (a.usePBR != b.usePBR);
											}

											if (root->HasComponent<LightComponent>() != baseRoot->HasComponent<LightComponent>())
												prefabMeta.overrideLight = true;
											else if (root->HasComponent<LightComponent>())
											{
												const auto& a = root->GetComponent<LightComponent>();
												const auto& b = baseRoot->GetComponent<LightComponent>();
												prefabMeta.overrideLight = (a.type != b.type) || !vec3Equal(a.color, b.color) || !vec3Equal(a.direction, b.direction) || !vec3Equal(a.position, b.position) || !nearlyEqual(a.intensity, b.intensity) || !nearlyEqual(a.range, b.range) || (a.castShadows != b.castShadows);
											}

											if (root->HasComponent<RigidbodyComponent>() != baseRoot->HasComponent<RigidbodyComponent>())
												prefabMeta.overrideRigidbody = true;
											else if (root->HasComponent<RigidbodyComponent>())
											{
												const auto& a = root->GetComponent<RigidbodyComponent>();
												const auto& b = baseRoot->GetComponent<RigidbodyComponent>();
												prefabMeta.overrideRigidbody = !vec3Equal(a.velocity, b.velocity) || !vec3Equal(a.acceleration, b.acceleration) || !nearlyEqual(a.mass, b.mass) || (a.isKinematic != b.isKinematic);
											}

											if (root->HasComponent<ScriptComponent>() != baseRoot->HasComponent<ScriptComponent>())
												prefabMeta.overrideScript = true;
											else if (root->HasComponent<ScriptComponent>())
											{
												const auto& a = root->GetComponent<ScriptComponent>();
												const auto& b = baseRoot->GetComponent<ScriptComponent>();
												prefabMeta.overrideScript = (a.scriptPath != b.scriptPath) || (a.enabled != b.enabled) || (a.autoStart != b.autoStart);
											}

											if (root->HasComponent<AnimationComponent>() != baseRoot->HasComponent<AnimationComponent>())
												prefabMeta.overrideAnimation = true;
											else if (root->HasComponent<AnimationComponent>())
											{
												const auto& a = root->GetComponent<AnimationComponent>();
												const auto& b = baseRoot->GetComponent<AnimationComponent>();
												prefabMeta.overrideAnimation = (a.activeClipIndex != b.activeClipIndex) || !nearlyEqual(a.time, b.time) || !nearlyEqual(a.playbackSpeed, b.playbackSpeed) || (a.playing != b.playing) || (a.looping != b.looping);
											}

											if (root->HasComponent<AudioSourceComponent>() != baseRoot->HasComponent<AudioSourceComponent>())
												prefabMeta.overrideAudioSource = true;
											else if (root->HasComponent<AudioSourceComponent>())
											{
												const auto& a = root->GetComponent<AudioSourceComponent>();
												const auto& b = baseRoot->GetComponent<AudioSourceComponent>();
												prefabMeta.overrideAudioSource =
													(a.clipPath != b.clipPath) ||
													!nearlyEqual(a.volume, b.volume) ||
													!nearlyEqual(a.pitch, b.pitch) ||
													(a.loop != b.loop) ||
													(a.autoPlay != b.autoPlay) ||
													(a.spatial != b.spatial) ||
													!nearlyEqual(a.minDistance, b.minDistance) ||
													!nearlyEqual(a.maxDistance, b.maxDistance) ||
													(a.busName != b.busName) ||
													(a.eventName != b.eventName);
											}

											if (root->HasComponent<AudioListenerComponent>() != baseRoot->HasComponent<AudioListenerComponent>())
												prefabMeta.overrideAudioListener = true;
											else if (root->HasComponent<AudioListenerComponent>())
											{
												const auto& a = root->GetComponent<AudioListenerComponent>();
												const auto& b = baseRoot->GetComponent<AudioListenerComponent>();
												prefabMeta.overrideAudioListener = (a.isPrimary != b.isPrimary) || !nearlyEqual(a.gain, b.gain);
											}

											auto meshTrianglesEqual = [&](const std::vector<std::array<glm::vec3, 3>>& lhs, const std::vector<std::array<glm::vec3, 3>>& rhs)
											{
												if (lhs.size() != rhs.size())
													return false;
												for (size_t tri = 0; tri < lhs.size(); ++tri)
												{
													for (int v = 0; v < 3; ++v)
													{
														if (!vec3Equal(lhs[tri][v], rhs[tri][v]))
															return false;
													}
												}
												return true;
											};

											if (root->HasComponent<BoxColliderComponent>() != baseRoot->HasComponent<BoxColliderComponent>())
												prefabMeta.overrideBoxCollider = true;
											else if (root->HasComponent<BoxColliderComponent>())
											{
												const auto& a = root->GetComponent<BoxColliderComponent>();
												const auto& b = baseRoot->GetComponent<BoxColliderComponent>();
												prefabMeta.overrideBoxCollider = !vec3Equal(a.center, b.center) || !vec3Equal(a.halfExtents, b.halfExtents) || (a.isTrigger != b.isTrigger);
											}

											if (root->HasComponent<CapsuleColliderComponent>() != baseRoot->HasComponent<CapsuleColliderComponent>())
												prefabMeta.overrideCapsuleCollider = true;
											else if (root->HasComponent<CapsuleColliderComponent>())
											{
												const auto& a = root->GetComponent<CapsuleColliderComponent>();
												const auto& b = baseRoot->GetComponent<CapsuleColliderComponent>();
												prefabMeta.overrideCapsuleCollider = !vec3Equal(a.pointA, b.pointA) || !vec3Equal(a.pointB, b.pointB) || !nearlyEqual(a.radius, b.radius) || (a.isTrigger != b.isTrigger);
											}

											if (root->HasComponent<PlaneColliderComponent>() != baseRoot->HasComponent<PlaneColliderComponent>())
												prefabMeta.overridePlaneCollider = true;
											else if (root->HasComponent<PlaneColliderComponent>())
											{
												const auto& a = root->GetComponent<PlaneColliderComponent>();
												const auto& b = baseRoot->GetComponent<PlaneColliderComponent>();
												prefabMeta.overridePlaneCollider = !vec3Equal(a.normal, b.normal) || !nearlyEqual(a.distance, b.distance) || (a.isTrigger != b.isTrigger);
											}

											if (root->HasComponent<BoundingSphereComponent>() != baseRoot->HasComponent<BoundingSphereComponent>())
												prefabMeta.overrideBoundingSphere = true;
											else if (root->HasComponent<BoundingSphereComponent>())
											{
												const auto& a = root->GetComponent<BoundingSphereComponent>();
												const auto& b = baseRoot->GetComponent<BoundingSphereComponent>();
												prefabMeta.overrideBoundingSphere = !vec3Equal(a.center, b.center) || !nearlyEqual(a.radius, b.radius) || (a.isTrigger != b.isTrigger);
											}

											if (root->HasComponent<MeshColliderComponent>() != baseRoot->HasComponent<MeshColliderComponent>())
												prefabMeta.overrideMeshCollider = true;
											else if (root->HasComponent<MeshColliderComponent>())
											{
												const auto& a = root->GetComponent<MeshColliderComponent>();
												const auto& b = baseRoot->GetComponent<MeshColliderComponent>();
												prefabMeta.overrideMeshCollider =
													(a.modelPath != b.modelPath) ||
													(a.isTrigger != b.isTrigger) ||
													!meshTrianglesEqual(a.triangles, b.triangles);
											}

											if (root->HasComponent<CharacterControllerComponent>() != baseRoot->HasComponent<CharacterControllerComponent>())
												prefabMeta.overrideCharacterController = true;
											else if (root->HasComponent<CharacterControllerComponent>())
											{
												const auto& a = root->GetComponent<CharacterControllerComponent>();
												const auto& b = baseRoot->GetComponent<CharacterControllerComponent>();
												prefabMeta.overrideCharacterController =
													!nearlyEqual(a.moveSpeed, b.moveSpeed) ||
													!nearlyEqual(a.airControl, b.airControl) ||
													!nearlyEqual(a.jumpSpeed, b.jumpSpeed) ||
													!nearlyEqual(a.gravityScale, b.gravityScale) ||
													!nearlyEqual(a.maxSlopeAngleDegrees, b.maxSlopeAngleDegrees) ||
													!nearlyEqual(a.groundSnapDistance, b.groundSnapDistance) ||
													!nearlyEqual(a.skinWidth, b.skinWidth) ||
													!nearlyEqual(a.maxStepHeight, b.maxStepHeight) ||
													!nearlyEqual(a.acceleration, b.acceleration) ||
													!nearlyEqual(a.airAcceleration, b.airAcceleration) ||
													!nearlyEqual(a.braking, b.braking) ||
													!nearlyEqual(a.slideGravityScale, b.slideGravityScale) ||
													(a.enableGroundSnap != b.enableGroundSnap) ||
													(a.orientToMovement != b.orientToMovement) ||
													(a.animationSpeedParameter != b.animationSpeedParameter) ||
													(a.animationGroundedParameter != b.animationGroundedParameter) ||
													(a.animationJumpTriggerParameter != b.animationJumpTriggerParameter);
											}

											auto vec4Equal = [&](const glm::vec4& lhs, const glm::vec4& rhs)
											{
												return nearlyEqual(lhs.x, rhs.x) && nearlyEqual(lhs.y, rhs.y) && nearlyEqual(lhs.z, rhs.z) && nearlyEqual(lhs.w, rhs.w);
											};

											if (root->HasComponent<NavigationAgentComponent>() != baseRoot->HasComponent<NavigationAgentComponent>())
												prefabMeta.overrideNavigationAgent = true;
											else if (root->HasComponent<NavigationAgentComponent>())
											{
												const auto& a = root->GetComponent<NavigationAgentComponent>();
												const auto& b = baseRoot->GetComponent<NavigationAgentComponent>();
												prefabMeta.overrideNavigationAgent = !nearlyEqual(a.speed, b.speed) || !nearlyEqual(a.stoppingDistance, b.stoppingDistance);
											}

											if (root->HasComponent<TerrainComponent>() != baseRoot->HasComponent<TerrainComponent>())
												prefabMeta.overrideTerrain = true;
											else if (root->HasComponent<TerrainComponent>())
											{
												const auto& a = root->GetComponent<TerrainComponent>();
												const auto& b = baseRoot->GetComponent<TerrainComponent>();
												prefabMeta.overrideTerrain =
													(a.heightmapPath != b.heightmapPath) ||
													!nearlyEqual(a.width, b.width) ||
													!nearlyEqual(a.depth, b.depth) ||
													!nearlyEqual(a.heightScale, b.heightScale) ||
													(a.resolution != b.resolution) ||
													(a.surfaceTexturePath != b.surfaceTexturePath) ||
													(a.paintResolution != b.paintResolution) ||
													(a.paintEnabled != b.paintEnabled) ||
													(a.paintActiveLayer != b.paintActiveLayer) ||
													!nearlyEqual(a.paintBrushRadius, b.paintBrushRadius) ||
													!nearlyEqual(a.paintBrushStrength, b.paintBrushStrength) ||
													!nearlyEqual(a.paintBrushFalloff, b.paintBrushFalloff) ||
													(a.paintLayers.size() != b.paintLayers.size()) ||
													(a.paintWeightData.size() != b.paintWeightData.size()) ||
													(a.shaderVertPath != b.shaderVertPath) ||
													(a.shaderFragPath != b.shaderFragPath) ||
													(a.sculptEnabled != b.sculptEnabled) ||
													!nearlyEqual(a.sculptBrushRadius, b.sculptBrushRadius) ||
													!nearlyEqual(a.sculptBrushStrength, b.sculptBrushStrength) ||
													!nearlyEqual(a.sculptBrushFalloff, b.sculptBrushFalloff) ||
													(a.sculptRaise != b.sculptRaise) ||
													(a.sculptBrushMode != b.sculptBrushMode) ||
													!nearlyEqual(a.sculptFlattenHeight, b.sculptFlattenHeight);
									}

									auto lodLevelsEqual = [&](const std::vector<LODComponent::Level>& lhs, const std::vector<LODComponent::Level>& rhs)
											{
												if (lhs.size() != rhs.size())
													return false;
												for (size_t i = 0; i < lhs.size(); ++i)
												{
													if (!nearlyEqual(lhs[i].distanceThreshold, rhs[i].distanceThreshold) || lhs[i].assetPath != rhs[i].assetPath)
														return false;
												}
												return true;
											};

											if (root->HasComponent<LODComponent>() != baseRoot->HasComponent<LODComponent>())
												prefabMeta.overrideLOD = true;
											else if (root->HasComponent<LODComponent>())
											{
												const auto& a = root->GetComponent<LODComponent>();
												const auto& b = baseRoot->GetComponent<LODComponent>();
												prefabMeta.overrideLOD = (a.enabled != b.enabled) || !lodLevelsEqual(a.levels, b.levels);
											}

											if (root->HasComponent<ParticleEmitterComponent>() != baseRoot->HasComponent<ParticleEmitterComponent>())
												prefabMeta.overrideParticleEmitter = true;
											else if (root->HasComponent<ParticleEmitterComponent>())
											{
												const auto& a = root->GetComponent<ParticleEmitterComponent>();
												const auto& b = baseRoot->GetComponent<ParticleEmitterComponent>();
												prefabMeta.overrideParticleEmitter =
													(a.maxParticles != b.maxParticles) ||
													!nearlyEqual(a.spawnRate, b.spawnRate) ||
													(a.emitting != b.emitting) ||
													(a.shape != b.shape) ||
													!nearlyEqual(a.shapeRadius, b.shapeRadius) ||
													!vec3Equal(a.shapeExtents, b.shapeExtents) ||
													!nearlyEqual(a.shapeHeight, b.shapeHeight) ||
													!vec4Equal(a.colorStart, b.colorStart) ||
													!vec4Equal(a.colorEnd, b.colorEnd) ||
													!nearlyEqual(a.sizeStart, b.sizeStart) ||
													!nearlyEqual(a.sizeEnd, b.sizeEnd) ||
													!nearlyEqual(a.lifetime, b.lifetime) ||
													!nearlyEqual(a.lifetimeVariance, b.lifetimeVariance) ||
													!vec3Equal(a.emitDirection, b.emitDirection) ||
													!nearlyEqual(a.emitSpeed, b.emitSpeed) ||
													!nearlyEqual(a.emitSpeedVariance, b.emitSpeedVariance) ||
													!nearlyEqual(a.spreadAngle, b.spreadAngle) ||
													!vec3Equal(a.gravity, b.gravity) ||
													(a.texturePath != b.texturePath);
											}

											prefabMeta.overrideCollisionEvents = (root->HasComponent<CollisionEventsComponent>() != baseRoot->HasComponent<CollisionEventsComponent>());
										}
									}

									return SaveScene(variantScene, variantPath);
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
											if (source->HasComponent<PrefabInstanceComponent>())
											{
												const auto& sourcePrefabMeta = source->GetComponent<PrefabInstanceComponent>();
												prefabInstance.isVariantInstance = sourcePrefabMeta.isVariantInstance ||
													(!sourcePrefabMeta.sourcePrefabPath.empty() && sourcePrefabMeta.sourceEntityID != 0);
												if (sourcePrefabMeta.isVariantInstance)
												{
															prefabInstance.variantBasePrefabPath = sourcePrefabMeta.variantBasePrefabPath;
															prefabInstance.variantBaseEntityID = sourcePrefabMeta.variantBaseEntityID;
															prefabInstance.overrideName = sourcePrefabMeta.overrideName;
															prefabInstance.overrideTag = sourcePrefabMeta.overrideTag;
															prefabInstance.overrideLayer = sourcePrefabMeta.overrideLayer;
															prefabInstance.overrideTransform = sourcePrefabMeta.overrideTransform;
															prefabInstance.overrideMeshRenderer = sourcePrefabMeta.overrideMeshRenderer;
															prefabInstance.overrideLight = sourcePrefabMeta.overrideLight;
															prefabInstance.overrideRigidbody = sourcePrefabMeta.overrideRigidbody;
															prefabInstance.overrideScript = sourcePrefabMeta.overrideScript;
															prefabInstance.overrideAnimation = sourcePrefabMeta.overrideAnimation;
															prefabInstance.overrideAudioSource = sourcePrefabMeta.overrideAudioSource;
															prefabInstance.overrideAudioListener = sourcePrefabMeta.overrideAudioListener;
															prefabInstance.overrideBoxCollider = sourcePrefabMeta.overrideBoxCollider;
															prefabInstance.overrideCapsuleCollider = sourcePrefabMeta.overrideCapsuleCollider;
															prefabInstance.overridePlaneCollider = sourcePrefabMeta.overridePlaneCollider;
															prefabInstance.overrideBoundingSphere = sourcePrefabMeta.overrideBoundingSphere;
															prefabInstance.overrideMeshCollider = sourcePrefabMeta.overrideMeshCollider;
															prefabInstance.overrideCharacterController = sourcePrefabMeta.overrideCharacterController;
															prefabInstance.overrideNavigationAgent = sourcePrefabMeta.overrideNavigationAgent;
															prefabInstance.overrideTerrain = sourcePrefabMeta.overrideTerrain;
															prefabInstance.overrideParticleEmitter = sourcePrefabMeta.overrideParticleEmitter;
															prefabInstance.overrideLOD = sourcePrefabMeta.overrideLOD;
															prefabInstance.overrideCollisionEvents = sourcePrefabMeta.overrideCollisionEvents;
														}
														else
														{
															prefabInstance.variantBasePrefabPath = sourcePrefabMeta.sourcePrefabPath;
															prefabInstance.variantBaseEntityID = sourcePrefabMeta.sourceEntityID;
														}
													}

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
											COPY_COMPONENT(CombatAttackComponent);
											COPY_COMPONENT(CombatStatsComponent);
											COPY_COMPONENT(BossAIComponent);
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
									COPY_COMPONENT(MovingPlatformComponent);
									COPY_COMPONENT(NavigationAgentComponent);
									COPY_COMPONENT(ParticleEmitterComponent);
									COPY_COMPONENT(CollisionEventsComponent);
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
