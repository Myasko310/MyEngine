#include "character/CharacterPreset.h"
#include "character/CharacterMaterialAssets.h"
#include "character/CharacterSocketSystem.h"
#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/CameraComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/CharacterCustomizationComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/SkeletonComponent.h"
#include "components/SocketAttachmentComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Entity.h"
#include "ecs/Scene.h"
#include "rendering/Skeleton.h"
#include "systems/AnimationSystem.h"
#include "systems/CameraSystem.h"
#include "editor/CharacterPreviewCamera.h"
#include "editor/PlayModeSession.h"
#include "editor/panels/CharacterEditorPanel.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <vector>

namespace
{
	bool NearlyEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0005f)
	{
		return glm::length(a - b) <= epsilon;
	}

	bool NearlyEqual(float a, float b, float epsilon = 0.0005f)
	{
		return std::abs(a - b) <= epsilon;
	}

	// Three-bone chain: Hips -> Spine -> Head, each 1 unit up from its parent
	// in bind pose, with offset matrices set to the inverse bind so the
	// skinning palette resolves to identity at rest.
	std::shared_ptr<MyEngine::Skeleton> BuildChainSkeleton()
	{
		auto skeleton = std::make_shared<MyEngine::Skeleton>();
		const char* names[] = { "Hips", "Spine", "Head" };
		glm::mat4 accumulated(1.0f);
		for (int i = 0; i < 3; ++i)
		{
			MyEngine::Bone bone;
			bone.name = names[i];
			bone.parentIndex = i - 1;
			bone.localBindTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, i == 0 ? 0.0f : 1.0f, 0.0f));
			accumulated = accumulated * bone.localBindTransform;
			bone.offsetMatrix = glm::inverse(accumulated);
			skeleton->AddBone(bone);
		}
		return skeleton;
	}

	glm::vec3 BoneWorldPosition(const AnimationComponent& anim, const MyEngine::Skeleton& skeleton, int boneIndex)
	{
		const glm::mat4 model = anim.boneMatrices[boneIndex] * glm::inverse(skeleton.GetBones()[boneIndex].offsetMatrix);
		return glm::vec3(model[3]);
	}

	bool OverrideLayerAffectsPoseTest()
	{
		Scene scene;
		auto entity = scene.CreateEntity("Player");
		auto& anim = entity->AddComponent<AnimationComponent>();
		auto& skel = entity->AddComponent<SkeletonComponent>();
		skel.skeleton = BuildChainSkeleton();

		anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		MyEngine::AnimationClip clip;
		clip.name = "Idle";
		clip.durationTicks = 10;
		clip.ticksPerSecond = 1;
		anim.clips->push_back(clip);

		MyEngine::AnimationSystem animation;
		animation.Update(scene, 0.0f);
		if (anim.boneMatrices.size() != 3) return false;

		// Baseline: bind pose => Head sits at y=2.
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 2.0f, 0.0f))) return false;

		// Translating Spine should carry Head along (child inherits parent delta).
		auto& customization = entity->AddComponent<CharacterCustomizationComponent>();
		customization.GetOrCreateOverride("Spine").positionOffset = glm::vec3(0.5f, 0.0f, 0.0f);
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 1), glm::vec3(0.5f, 1.0f, 0.0f))) return false;
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.5f, 2.0f, 0.0f))) return false;
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 0), glm::vec3(0.0f))) return false;

		// Scaling Spine by 2 doubles the Head's local offset from Spine.
		customization.ResetAll();
		customization.GetOrCreateOverride("Spine").scaleMultiplier = glm::vec3(2.0f);
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 3.0f, 0.0f))) return false;

		// Disabling the layer restores the animated pose without clearing data.
		customization.enabled = false;
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 2.0f, 0.0f))) return false;
		if (!customization.HasAnyOverride()) return false;
		customization.enabled = true;

		// Overrides must not leak into the blend source snapshot across a transition.
		MyEngine::AnimationClip second = clip;
		second.name = "Other";
		anim.clips->push_back(second);
		anim.TransitionTo(1, 0.5f);
		animation.Update(scene, 0.25f);
		if (!anim.blending) return false;
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 3.0f, 0.0f))) return false;
		if (anim.localPose.size() != 3) return false;
		if (!NearlyEqual(glm::vec3(anim.localPose[1][3]), glm::vec3(0.0f, 1.0f, 0.0f))) return false;
		if (!NearlyEqual(glm::length(glm::vec3(anim.localPose[1][1])), 1.0f)) return false;

		std::cout << "[CharacterEditor] Override layer translation/scale/disable/blend isolation passed" << std::endl;
		return true;
	}

	bool PresetRoundTripTest()
	{
		CharacterCustomizationComponent original;
		original.GetOrCreateOverride("Spine").positionOffset = glm::vec3(0.1f, -0.2f, 0.3f);
		original.GetOrCreateOverride("Spine").rotationOffsetDegrees = glm::vec3(10.0f, 20.0f, -30.0f);
		original.GetOrCreateOverride("Head").scaleMultiplier = glm::vec3(1.25f, 1.5f, 0.75f);
		original.GetOrCreateOverride("Hips");

		const std::string json = MyEngine::CharacterPreset::ToJson(original, BuildChainSkeleton().get());
		if (json.find("\"CharacterPreset\"") == std::string::npos) return false;
		if (json.find("\"skeletonBoneCount\": 3") == std::string::npos) return false;
		if (json.find("\"Hips\"") != std::string::npos) return false;

		CharacterCustomizationComponent restored;
		if (!MyEngine::CharacterPreset::FromJson(json, restored)) return false;
		if (restored.boneOverrides.size() != 2) return false;
		const BoneOverride* spine = restored.FindOverride("Spine");
		const BoneOverride* head = restored.FindOverride("Head");
		if (!spine || !head) return false;
		if (!NearlyEqual(spine->positionOffset, glm::vec3(0.1f, -0.2f, 0.3f))) return false;
		if (!NearlyEqual(spine->rotationOffsetDegrees, glm::vec3(10.0f, 20.0f, -30.0f))) return false;
		if (!NearlyEqual(spine->scaleMultiplier, glm::vec3(1.0f))) return false;
		if (!NearlyEqual(head->scaleMultiplier, glm::vec3(1.25f, 1.5f, 0.75f))) return false;

		const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "myengine_character_editor_smoke.character.json";
		if (!MyEngine::CharacterPreset::Save(original, tempPath.string())) return false;
		CharacterCustomizationComponent loaded;
		if (!MyEngine::CharacterPreset::Load(tempPath.string(), loaded)) return false;
		std::filesystem::remove(tempPath);
		if (loaded.presetPath != tempPath.string()) return false;
		if (loaded.boneOverrides.size() != 2) return false;

		CharacterCustomizationComponent rejected;
		if (MyEngine::CharacterPreset::FromJson("{ not json", rejected)) return false;
		if (MyEngine::CharacterPreset::FromJson("{\"type\":\"Material\"}", rejected)) return false;

		std::cout << "[CharacterEditor] Preset JSON/file round-trip and rejection passed" << std::endl;
		return true;
	}

	bool DeltaMatrixCompositionTest()
	{
		BoneOverride boneOverride;
		boneOverride.positionOffset = glm::vec3(1.0f, 2.0f, 3.0f);
		boneOverride.rotationOffsetDegrees = glm::vec3(0.0f, 90.0f, 0.0f);
		boneOverride.scaleMultiplier = glm::vec3(2.0f);

		const glm::mat4 delta = boneOverride.BuildDeltaMatrix();
		// T * R * S: a unit X point scales to 2, rotates +90deg about Y to -Z, then translates.
		const glm::vec3 transformed = glm::vec3(delta * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		if (!NearlyEqual(transformed, glm::vec3(1.0f, 2.0f, 1.0f))) return false;
		if (!BoneOverride{}.IsIdentity() || boneOverride.IsIdentity()) return false;

		std::cout << "[CharacterEditor] BoneOverride delta composition passed" << std::endl;
		return true;
	}

	bool BodySliderCompositionTest()
	{
		CharacterCustomizationComponent customization;
		BodySlider slider;
		slider.name = "Torso Length";
		BodySliderBoneMapping mapping;
		mapping.boneName = "Spine";
		mapping.scalePerUnit = glm::vec3(0.0f, 0.5f, 0.0f);
		mapping.positionPerUnit = glm::vec3(0.0f, 0.2f, 0.0f);
		slider.mappings.push_back(mapping);
		customization.bodySliders.push_back(slider);

		// Default value => no influence.
		if (!customization.ComputeEffectiveOverride("Spine").IsIdentity()) return false;
		if (customization.HasAnyOverride()) return false;

		customization.FindSlider("Torso Length")->value = 1.0f;
		BoneOverride effective = customization.ComputeEffectiveOverride("Spine");
		if (!NearlyEqual(effective.scaleMultiplier, glm::vec3(1.0f, 1.5f, 1.0f))) return false;
		if (!NearlyEqual(effective.positionOffset, glm::vec3(0.0f, 0.2f, 0.0f))) return false;
		if (!customization.ComputeEffectiveOverride("Head").IsIdentity()) return false;

		// Manual override composes on top (scale multiplicative, position additive).
		customization.GetOrCreateOverride("Spine").scaleMultiplier = glm::vec3(2.0f, 2.0f, 2.0f);
		customization.GetOrCreateOverride("Spine").positionOffset = glm::vec3(0.1f, 0.0f, 0.0f);
		effective = customization.ComputeEffectiveOverride("Spine");
		if (!NearlyEqual(effective.scaleMultiplier, glm::vec3(2.0f, 3.0f, 2.0f))) return false;
		if (!NearlyEqual(effective.positionOffset, glm::vec3(0.1f, 0.2f, 0.0f))) return false;

		// Slider drives the animated pose through the system.
		Scene scene;
		auto entity = scene.CreateEntity("Player");
		auto& anim = entity->AddComponent<AnimationComponent>();
		auto& skel = entity->AddComponent<SkeletonComponent>();
		skel.skeleton = BuildChainSkeleton();
		anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		MyEngine::AnimationClip clip;
		clip.name = "Idle";
		clip.durationTicks = 10;
		clip.ticksPerSecond = 1;
		anim.clips->push_back(clip);

		auto& live = entity->AddComponent<CharacterCustomizationComponent>();
		live.bodySliders.push_back(slider);
		live.FindSlider("Torso Length")->value = 1.0f;

		MyEngine::AnimationSystem animation;
		animation.Update(scene, 0.0f);
		// Spine scaled 1.5 in Y and lifted 0.2 => Head (1 unit above Spine) at 1 + 0.2 + 1.5 = 2.7.
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 2.7f, 0.0f))) return false;

		// Defaults factory is idempotent and preserves values.
		CharacterCustomizationComponent defaults;
		defaults.EnsureDefaultBodySliders();
		const size_t count = defaults.bodySliders.size();
		if (count == 0 || !defaults.FindSlider("Height")) return false;
		defaults.FindSlider("Height")->value = 0.5f;
		defaults.EnsureDefaultBodySliders();
		if (defaults.bodySliders.size() != count || !NearlyEqual(defaults.FindSlider("Height")->value, 0.5f)) return false;

		std::cout << "[CharacterEditor] Body slider composition passed" << std::endl;
		return true;
	}

	bool PresetV2RoundTripTest()
	{
		CharacterCustomizationComponent original;
		original.EnsureDefaultBodySliders();
		original.FindSlider("Height")->value = 0.35f;
		original.FindSlider("Head Size")->value = -0.5f;

		BoneSocket socket;
		socket.name = "RightHandWeapon";
		socket.boneName = "Head";
		socket.meshPath = "primitive_cube";
		socket.positionOffset = glm::vec3(0.1f, 0.2f, 0.3f);
		socket.rotationOffsetDegrees = glm::vec3(0.0f, 45.0f, 0.0f);
		socket.scale = glm::vec3(0.25f);
		socket.visible = false;
		original.sockets.push_back(socket);

		const std::string json = MyEngine::CharacterPreset::ToJson(original, nullptr);
		if (json.find("\"version\": 3") == std::string::npos) return false;
		if (json.find("\"Height\"") == std::string::npos) return false;
		if (json.find("\"Torso Length\"") != std::string::npos) return false; // zero sliders omitted
		if (json.find("\"RightHandWeapon\"") == std::string::npos) return false;

		CharacterCustomizationComponent restored;
		if (!MyEngine::CharacterPreset::FromJson(json, restored)) return false;
		if (!restored.FindSlider("Height") || !NearlyEqual(restored.FindSlider("Height")->value, 0.35f)) return false;
		if (!restored.FindSlider("Head Size") || !NearlyEqual(restored.FindSlider("Head Size")->value, -0.5f)) return false;
		if (restored.FindSlider("Height")->mappings.empty()) return false; // defaults re-attached
		if (restored.sockets.size() != 1) return false;
		const BoneSocket& s = restored.sockets.front();
		if (s.name != "RightHandWeapon" || s.boneName != "Head" || s.meshPath != "primitive_cube") return false;
		if (!NearlyEqual(s.positionOffset, glm::vec3(0.1f, 0.2f, 0.3f))) return false;
		if (!NearlyEqual(s.rotationOffsetDegrees, glm::vec3(0.0f, 45.0f, 0.0f))) return false;
		if (!NearlyEqual(s.scale, glm::vec3(0.25f)) || s.visible) return false;

		// v1 documents (no sliders/sockets) still load.
		CharacterCustomizationComponent legacy;
		if (!MyEngine::CharacterPreset::FromJson("{\"type\":\"CharacterPreset\",\"version\":1,\"boneOverrides\":[]}", legacy)) return false;
		if (!legacy.sockets.empty()) return false;

		std::cout << "[CharacterEditor] Preset v2 slider/socket round-trip passed" << std::endl;
		return true;
	}

	bool SocketSystemTest()
	{
		Scene scene;
		auto owner = scene.CreateEntity("Player");
		auto& ownerTransform = owner->AddComponent<TransformComponent>();
		ownerTransform.position = glm::vec3(10.0f, 0.0f, 0.0f);
		auto& anim = owner->AddComponent<AnimationComponent>();
		auto& skel = owner->AddComponent<SkeletonComponent>();
		skel.skeleton = BuildChainSkeleton();
		anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		MyEngine::AnimationClip clip;
		clip.name = "Idle";
		clip.durationTicks = 10;
		clip.ticksPerSecond = 1;
		anim.clips->push_back(clip);

		auto& customization = owner->AddComponent<CharacterCustomizationComponent>();
		BoneSocket socket;
		socket.name = "Hat";
		socket.boneName = "Head";
		socket.meshPath = "test_mesh";
		socket.positionOffset = glm::vec3(0.0f, 0.5f, 0.0f);
		socket.scale = glm::vec3(0.5f);
		customization.sockets.push_back(socket);

		MyEngine::AnimationSystem animation;
		animation.Update(scene, 0.0f);

		// Headless: no GL, so the loader yields no mesh; the system must still
		// create and place the attachment entity.
		int loaderCalls = 0;
		MyEngine::CharacterSocketSystem sockets;
		sockets.SetMeshLoader([&loaderCalls](const std::string&) { ++loaderCalls; return std::shared_ptr<MyEngine::Mesh>(); });

		sockets.Update(scene);
		if (scene.GetEntities().size() != 2) return false;
		if (loaderCalls != 1) return false;

		std::shared_ptr<Entity> attachment;
		for (const auto& e : scene.GetEntities())
		{
			if (e->HasComponent<SocketAttachmentComponent>())
				attachment = e;
		}
		if (!attachment) return false;
		const auto& marker = attachment->GetComponent<SocketAttachmentComponent>();
		if (marker.ownerEntityID != owner->GetID() || marker.socketName != "Hat") return false;

		// Head at (0,2,0) in model space + 0.5 up, owner at x=10 => (10, 2.5, 0).
		const auto& t = attachment->GetComponent<TransformComponent>();
		if (!NearlyEqual(t.position, glm::vec3(10.0f, 2.5f, 0.0f))) return false;
		if (!NearlyEqual(t.scale, glm::vec3(0.5f))) return false;
		if (t.parentID != 0) return false;

		// Second update: no duplicates, loader not re-invoked for the same path.
		sockets.Update(scene);
		if (scene.GetEntities().size() != 2 || loaderCalls != 1) return false;

		// Mesh path change triggers a reload.
		customization.sockets.front().meshPath = "other_mesh";
		sockets.Update(scene);
		if (loaderCalls != 2) return false;

		// Owner moves and the slider lifts the head: attachment follows.
		ownerTransform.position.z = 3.0f;
		customization.GetOrCreateOverride("Spine").positionOffset = glm::vec3(0.0f, 1.0f, 0.0f);
		animation.Update(scene, 0.0f);
		sockets.Update(scene);
		if (!NearlyEqual(attachment->GetComponent<TransformComponent>().position, glm::vec3(10.0f, 3.5f, 3.0f))) return false;

		// Static world matrix helper agrees.
		glm::mat4 world;
		if (!MyEngine::CharacterSocketSystem::ComputeSocketWorldMatrix(scene, *owner, "Head", customization.sockets.front().BuildLocalMatrix(), world)) return false;
		if (!NearlyEqual(glm::vec3(world[3]), glm::vec3(10.0f, 3.5f, 3.0f))) return false;
		if (MyEngine::CharacterSocketSystem::ComputeSocketWorldMatrix(scene, *owner, "NoSuchBone", glm::mat4(1.0f), world)) return false;

		// Removing the socket destroys the orphaned attachment.
		customization.sockets.clear();
		sockets.Update(scene);
		if (scene.GetEntities().size() != 1) return false;

		// Removing the owner destroys attachments too.
		customization.sockets.push_back(socket);
		sockets.Update(scene);
		if (scene.GetEntities().size() != 2) return false;
		scene.DestroyEntity(owner->GetID());
		sockets.Update(scene);
		if (!scene.GetEntities().empty()) return false;

		std::cout << "[CharacterEditor] Socket system create/follow/reload/orphan cleanup passed" << std::endl;
		return true;
	}

	bool SliderMappingEditorTest()
	{
		CharacterCustomizationComponent customization;
		customization.EnsureDefaultBodySliders();
		const size_t defaultCount = customization.bodySliders.size();

		// Custom slider creation rejects empty/duplicate names.
		if (customization.AddCustomSlider("")) return false;
		if (customization.AddCustomSlider("Height")) return false;
		BodySlider* custom = customization.AddCustomSlider("Jaw Width");
		if (!custom || !custom->customMappings || customization.bodySliders.size() != defaultCount + 1) return false;
		BodySliderBoneMapping mapping;
		mapping.boneName = "Spine";
		mapping.positionPerUnit = glm::vec3(0.0f, 0.3f, 0.0f);
		mapping.rotationDegreesPerUnit = glm::vec3(10.0f, 0.0f, 0.0f);
		custom->mappings.push_back(mapping);
		custom->minValue = 0.0f;
		custom->maxValue = 2.0f;
		custom->value = 1.5f;

		// Editing a default slider's mappings marks it custom and survives EnsureDefaultBodySliders.
		BodySlider* height = customization.FindSlider("Height");
		height->mappings.clear();
		BodySliderBoneMapping headOnly;
		headOnly.boneName = "Head";
		headOnly.scalePerUnit = glm::vec3(0.5f);
		height->mappings.push_back(headOnly);
		height->customMappings = true;
		height->value = 1.0f;
		customization.EnsureDefaultBodySliders();
		if (customization.bodySliders.size() != defaultCount + 1) return false;
		if (customization.FindSlider("Height")->mappings.size() != 1 || customization.FindSlider("Height")->mappings.front().boneName != "Head") return false;
		if (!NearlyEqual(customization.ComputeSliderOverride("Head").scaleMultiplier, glm::vec3(1.5f))) return false;
		if (!customization.ComputeSliderOverride("Hips").IsIdentity()) return false;

		// Full definition round-trips through the preset (v3).
		const std::string json = MyEngine::CharacterPreset::ToJson(customization, nullptr);
		if (json.find("\"version\": 3") == std::string::npos) return false;
		if (json.find("\"Jaw Width\"") == std::string::npos || json.find("\"custom\": true") == std::string::npos) return false;
		if (json.find("\"Torso Length\"") != std::string::npos) return false; // untouched default at 0 still omitted

		CharacterCustomizationComponent restored;
		if (!MyEngine::CharacterPreset::FromJson(json, restored)) return false;
		const BodySlider* restoredCustom = restored.FindSlider("Jaw Width");
		if (!restoredCustom || !restoredCustom->customMappings) return false;
		if (!NearlyEqual(restoredCustom->value, 1.5f) || !NearlyEqual(restoredCustom->minValue, 0.0f) || !NearlyEqual(restoredCustom->maxValue, 2.0f)) return false;
		if (restoredCustom->mappings.size() != 1 || restoredCustom->mappings.front().boneName != "Spine") return false;
		if (!NearlyEqual(restoredCustom->mappings.front().positionPerUnit, glm::vec3(0.0f, 0.3f, 0.0f))) return false;
		if (!NearlyEqual(restoredCustom->mappings.front().rotationDegreesPerUnit, glm::vec3(10.0f, 0.0f, 0.0f))) return false;
		const BodySlider* restoredHeight = restored.FindSlider("Height");
		if (!restoredHeight || !restoredHeight->customMappings || restoredHeight->mappings.size() != 1 || restoredHeight->mappings.front().boneName != "Head") return false;
		if (restored.FindSlider("Torso Length")->mappings.empty()) return false; // defaults still attached

		// Loading a preset without the custom slider drops the stale definition.
		if (!MyEngine::CharacterPreset::FromJson("{\"type\":\"CharacterPreset\",\"version\":2,\"bodySliders\":[{\"name\":\"Height\",\"value\":0.25}]}", restored)) return false;
		if (restored.FindSlider("Height")->customMappings) return false;
		if (restored.FindSlider("Height")->mappings.front().boneName != "Hips") return false; // default mapping restored
		if (restored.FindSlider("Jaw Width") && !restored.FindSlider("Jaw Width")->mappings.empty()) return false;

		// Reset-to-default and removal.
		if (!customization.ResetSliderToDefault("Height")) return false;
		if (customization.FindSlider("Height")->customMappings || customization.FindSlider("Height")->mappings.front().boneName != "Hips") return false;
		if (customization.ResetSliderToDefault("Jaw Width")) return false;
		if (!customization.RemoveSlider("Jaw Width") || customization.FindSlider("Jaw Width")) return false;
		if (customization.RemoveSlider("Jaw Width")) return false;

		std::cout << "[CharacterEditor] Slider mapping editor model/preset v3 passed" << std::endl;
		return true;
	}

	bool PresetLibraryTest()
	{
		namespace Preset = MyEngine::CharacterPreset;
		namespace fs = std::filesystem;

		const fs::path dir = fs::temp_directory_path() / "myengine_preset_library_test";
		std::error_code ec;
		fs::remove_all(dir, ec);
		const std::string directory = dir.generic_string();

		if (!Preset::ListPresets(directory).empty()) return false; // missing dir => empty
		if (Preset::PresetDisplayName("assets/characters/hero.character.json") != "hero") return false;
		if (Preset::MakePresetPath(directory, "hero") != directory + "/hero.character.json") return false;
		if (Preset::MakePresetPath(directory, "hero.character.json") != directory + "/hero.character.json") return false;
		if (!Preset::MakePresetPath(directory, "").empty()) return false;

		CharacterCustomizationComponent component;
		component.GetOrCreateOverride("Head").scaleMultiplier = glm::vec3(1.2f);
		const std::string hero = Preset::MakePresetPath(directory, "hero");
		const std::string villain = Preset::MakePresetPath(directory, "villain");
		if (!Preset::Save(component, hero) || !Preset::Save(component, villain)) return false;
		// Non-preset files are ignored.
		{ std::ofstream junk(dir / "notes.txt"); junk << "x"; }

		std::vector<std::string> list = Preset::ListPresets(directory);
		if (list.size() != 2 || list[0] != hero || list[1] != villain) return false;

		const std::string copy = Preset::Duplicate(hero);
		if (copy != Preset::MakePresetPath(directory, "hero Copy")) return false;
		const std::string copy2 = Preset::Duplicate(hero);
		if (copy2 != Preset::MakePresetPath(directory, "hero Copy 2")) return false;
		if (!Preset::Duplicate(hero, "villain").empty()) return false; // collision rejected
		const std::string named = Preset::Duplicate(hero, "sidekick");
		if (named != Preset::MakePresetPath(directory, "sidekick")) return false;
		CharacterCustomizationComponent loaded;
		if (!Preset::Load(named, loaded) || !loaded.FindOverride("Head")) return false;

		if (!Preset::Rename(copy2, "villain").empty()) return false; // collision rejected
		if (!Preset::Rename(copy2, "").empty()) return false;
		const std::string renamed = Preset::Rename(copy2, "henchman");
		if (renamed != Preset::MakePresetPath(directory, "henchman") || fs::exists(copy2)) return false;

		if (!Preset::Delete(renamed) || fs::exists(renamed)) return false;
		if (Preset::Delete(renamed)) return false; // already gone
		if (Preset::Delete((dir / "notes.txt").generic_string())) return false; // refuses non-presets
		if (!fs::exists(dir / "notes.txt")) return false;

		list = Preset::ListPresets(directory);
		if (list.size() != 4) return false; // hero, hero Copy, sidekick, villain

		fs::remove_all(dir, ec);
		std::cout << "[CharacterEditor] Preset library list/duplicate/rename/delete passed" << std::endl;
		return true;
	}

	bool CounterScaleChildrenTest()
	{
		Scene scene;
		auto entity = scene.CreateEntity("Player");
		auto& anim = entity->AddComponent<AnimationComponent>();
		auto& skel = entity->AddComponent<SkeletonComponent>();
		skel.skeleton = BuildChainSkeleton();
		anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		MyEngine::AnimationClip clip;
		clip.name = "Idle";
		clip.durationTicks = 10;
		clip.ticksPerSecond = 1;
		anim.clips->push_back(clip);
		auto& customization = entity->AddComponent<CharacterCustomizationComponent>();
		MyEngine::AnimationSystem animation;

		auto headScale = [&]()
		{
			const glm::mat4 model = anim.boneMatrices[2] * glm::inverse(skel.skeleton->GetBones()[2].offsetMatrix);
			return glm::vec3(glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])), glm::length(glm::vec3(model[2])));
		};

		// Without the flag, Spine's scale propagates to Head.
		BoneOverride& spine = customization.GetOrCreateOverride("Spine");
		spine.scaleMultiplier = glm::vec3(2.0f, 1.0f, 2.0f);
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(headScale(), glm::vec3(2.0f, 1.0f, 2.0f))) return false;

		// With the flag, Head keeps unit scale but still sits on top of Spine.
		spine.counterScaleChildren = true;
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(headScale(), glm::vec3(1.0f))) return false;
		if (!NearlyEqual(BoneWorldPosition(anim, *skel.skeleton, 2), glm::vec3(0.0f, 2.0f, 0.0f))) return false;
		// Head's own offset is in Spine's scaled frame, but the compensation
		// prepended to Head's local cancels it for Head's children too.

		// Slider mapping variant.
		customization.ResetAll();
		BodySlider slider;
		slider.name = "Spine Bulk";
		BodySliderBoneMapping mapping;
		mapping.boneName = "Spine";
		mapping.scalePerUnit = glm::vec3(0.5f, 0.0f, 0.5f);
		mapping.counterScaleChildren = true;
		slider.mappings.push_back(mapping);
		customization.bodySliders.push_back(slider);
		customization.FindSlider("Spine Bulk")->value = 1.0f;
		animation.Update(scene, 0.0f);
		if (!NearlyEqual(headScale(), glm::vec3(1.0f))) return false;
		const glm::mat4 spineModel = anim.boneMatrices[1] * glm::inverse(skel.skeleton->GetBones()[1].offsetMatrix);
		if (!NearlyEqual(glm::length(glm::vec3(spineModel[0])), 1.5f)) return false;

		// Flags round-trip through presets.
		customization.bodySliders.front().customMappings = true;
		customization.GetOrCreateOverride("Head").scaleMultiplier = glm::vec3(1.1f);
		customization.GetOrCreateOverride("Head").counterScaleChildren = true;
		CharacterCustomizationComponent restored;
		if (!MyEngine::CharacterPreset::FromJson(MyEngine::CharacterPreset::ToJson(customization), restored)) return false;
		if (!restored.FindOverride("Head") || !restored.FindOverride("Head")->counterScaleChildren) return false;
		if (!restored.FindSlider("Spine Bulk") || !restored.FindSlider("Spine Bulk")->mappings.front().counterScaleChildren) return false;

		std::cout << "[CharacterEditor] Child counter-scale passed" << std::endl;
		return true;
	}
}

static bool CharacterMaterialTest()
{
	CharacterCustomizationComponent original;
	auto& material = original.bodyMaterial;
	material.tint = glm::vec3(0.5f, 0.25f, 1.0f);
	material.overrideShininess = true;
	material.shininess = 80.0f;
	material.overridePBR = true;
	material.metallic = 0.8f;
	material.roughness = 0.2f;
	material.emissive = glm::vec3(2.0f, 0.0f, 1.0f);
	glm::vec3 albedo(0.8f), emissive(0.1f);
	float shininess = 32.0f, metallic = 0.0f, roughness = 0.5f;
	material.Apply(albedo, shininess, metallic, roughness, emissive);
	if (!NearlyEqual(albedo, glm::vec3(0.8f)) || !NearlyEqual(shininess, 32.0f)
		|| !NearlyEqual(metallic, 0.0f) || !NearlyEqual(roughness, 0.5f)
		|| !NearlyEqual(emissive, glm::vec3(0.1f))) return false;
	material.enabled = true;
	material.Apply(albedo, shininess, metallic, roughness, emissive);
	if (!NearlyEqual(albedo, glm::vec3(0.4f, 0.2f, 0.8f)) || !NearlyEqual(shininess, 80.0f)
		|| !NearlyEqual(metallic, 0.8f) || !NearlyEqual(roughness, 0.2f)
		|| !NearlyEqual(emissive, material.emissive)) return false;

	BoneSocket socket;
	socket.name = "Weapon";
	socket.boneName = "Head";
	socket.material.enabled = true;
	socket.material.tint = glm::vec3(0.1f, 1.0f, 0.3f);
	original.sockets.push_back(socket);
	CharacterCustomizationComponent restored;
	const std::string json = MyEngine::CharacterPreset::ToJson(original);
	if (!MyEngine::CharacterPreset::FromJson(json, restored)) return false;
	if (MyEngine::CharacterPreset::ToJson(restored) != json) return false;
	if (restored.sockets.size() != 1 || !restored.bodyMaterial.enabled
		|| !restored.sockets[0].material.enabled) return false;
	albedo = glm::vec3(1.0f);
	restored.sockets[0].material.Apply(albedo, shininess, metallic, roughness, emissive);
	if (!NearlyEqual(albedo, socket.material.tint) || !NearlyEqual(shininess, 80.0f)
		|| !NearlyEqual(metallic, 0.8f)) return false;
	restored.sockets[0].material = {};
	if (!restored.bodyMaterial.enabled || !NearlyEqual(restored.bodyMaterial.tint, material.tint)) return false;

	const std::string before = MyEngine::CharacterPreset::ToJson(restored);
	const char* invalid[] = {
		R"({"bodyMaterial":{"tint":[1,0]}})",
		R"({"bodyMaterial":{"metallic":2}})",
		R"({"bodyMaterial":{"roughness":1e100}})",
		R"({"bodyMaterial":{"enabled":"true"}})",
		R"({"sockets":[{"name":"Weapon","material":{"emissive":[-1,0,0]}}]})"
	};
	for (const char* input : invalid)
	{
		if (MyEngine::CharacterPreset::FromJson(input, restored)) return false;
		if (MyEngine::CharacterPreset::ToJson(restored) != before) return false;
	}
	for (int version = 1; version <= 3; ++version)
	{
		restored.bodyMaterial = material;
		if (!MyEngine::CharacterPreset::FromJson("{\"version\":" + std::to_string(version) + "}", restored)) return false;
		if (restored.bodyMaterial.enabled || !restored.sockets.empty()) return false;
	}
	std::cout << "[CharacterEditor] Body/socket material application, preset compatibility and validation passed" << std::endl;
	return true;
}

static bool CharacterPreviewCameraTest()
{
	MyEngine::Editor::CharacterPreviewCamera camera;
	for (float aspect : { 0.5f, 1.0f, 2.0f })
	{
		camera.Focus(glm::vec3(-1, 0, -1), glm::vec3(1, 4, 1), aspect);
		if (!NearlyEqual(camera.target, glm::vec3(0, 2, 0))) return false;
		for (int corner = 0; corner < 8; ++corner)
		{
			glm::vec4 point((corner & 1) ? 1.0f : -1.0f, (corner & 2) ? 4.0f : 0.0f,
				(corner & 4) ? 1.0f : -1.0f, 1.0f);
			const glm::vec4 clip = camera.GetProjection(aspect) * camera.GetView() * point;
			if (clip.w <= 0 || std::abs(clip.x / clip.w) > 1 || std::abs(clip.y / clip.w) > 1
				|| std::abs(clip.z / clip.w) > 1) return false;
		}
	}
	camera.Orbit(10000.0f, 10000.0f);
	if (std::abs(camera.yaw) > 180 || camera.pitch != 85.0f) return false;
	camera.Orbit(0, -10000.0f);
	if (camera.pitch != -85.0f) return false;
	for (int i = 0; i < 20; ++i) camera.Zoom(20);
	if (!NearlyEqual(camera.distance, camera.radius * 0.2f)) return false;
	for (int i = 0; i < 20; ++i) camera.Zoom(-20);
	if (!NearlyEqual(camera.distance, camera.radius * 200)) return false;
	camera.Focus(glm::vec3(2), glm::vec3(2), 0);
	const auto view = camera.GetView();
	const auto projection = camera.GetProjection(0);
	for (int c = 0; c < 4; ++c)
		for (int r = 0; r < 4; ++r)
			if (!std::isfinite(view[c][r]) || !std::isfinite(projection[c][r])) return false;

#ifdef USE_IMGUI
	Scene scene;
	auto first = scene.CreateEntity("Preview A");
	first->AddComponent<TransformComponent>();
	first->AddComponent<SkeletonComponent>().skeleton = BuildChainSkeleton();
	auto second = scene.CreateEntity("Preview B");
	second->AddComponent<TransformComponent>().position = glm::vec3(10, 0, 0);
	second->AddComponent<SkeletonComponent>().skeleton = BuildChainSkeleton();
	MyEngine::Editor::Panels::CharacterEditorPanelState state;
	state.previewEnabled = true;
	bool open = true;
	MyEngine::Editor::Panels::CharacterEditorPanelContext context;
	context.isOpen = &open;
	context.scene = &scene;
	context.state = &state;
	context.selectedEntity = first.get();
	context.viewportWidth = 800;
	context.viewportHeight = 600;
	if (!MyEngine::Editor::Panels::UpdateCharacterPreviewCamera(context)) return false;
	const glm::vec3 firstTarget = state.previewCamera.target;
	context.selectedEntity = second.get();
	if (!MyEngine::Editor::Panels::UpdateCharacterPreviewCamera(context)) return false;
	if (!NearlyEqual(state.previewCamera.target - firstTarget, glm::vec3(10, 0, 0))) return false;
	if (!NearlyEqual(second->GetComponent<TransformComponent>().position, glm::vec3(10, 0, 0))) return false;
	context.previewAllowed = false;
	if (MyEngine::Editor::Panels::UpdateCharacterPreviewCamera(context) || state.previewEnabled) return false;
	context.previewAllowed = true;
	state.previewEnabled = true;
	open = false;
	if (MyEngine::Editor::Panels::UpdateCharacterPreviewCamera(context) || state.previewEnabled) return false;
#endif
	std::cout << "[CharacterEditor] Preview camera framing/orbit/zoom and lifecycle passed" << std::endl;
	return true;
}

static bool CharacterPickerTest()
{
#ifdef USE_IMGUI
	using namespace MyEngine::Editor::Panels;
	Scene scene;
	auto player = scene.CreateEntity("Player");
	player->AddComponent<SkeletonComponent>().skeleton = BuildChainSkeleton();
	auto mannequin = scene.CreateEntity("Character Editor Test");
	mannequin->AddComponent<SkeletonComponent>().skeleton = BuildChainSkeleton();
	auto empty = scene.CreateEntity("Empty Rig");
	empty->AddComponent<SkeletonComponent>();
	CharacterEditorPanelState state;
	CharacterEditorPanelContext context;
	context.scene = &scene;
	context.state = &state;
	context.playerEntity = &player;
	if (!SelectCharacterEditorTarget(context, player->GetID())) return false;
	if (context.selectedEntity != player.get() || std::string(state.presetPathBuffer) != MyEngine::CharacterPreset::kDefaultPlayerPresetPath) return false;
	state.selectedBoneIndex = 2;
	state.selectedSocketIndex = 0;
	state.editingSliderIndex = 1;
	state.selectedMappingIndex = 1;
	state.gizmoWasUsing = true;
	state.previewFocusRequested = false;
	if (!SelectCharacterEditorTarget(context, player->GetID()) || state.selectedBoneIndex != 2) return false;
	if (!SelectCharacterEditorTarget(context, mannequin->GetID())) return false;
	if (context.selectedEntity != mannequin.get() || state.selectedBoneIndex != -1 || state.selectedSocketIndex != -1
		|| state.editingSliderIndex != -1 || state.selectedMappingIndex != -1 || state.gizmoWasUsing || !state.previewFocusRequested) return false;
	if (std::string(state.presetPathBuffer) == MyEngine::CharacterPreset::kDefaultPlayerPresetPath) return false;
	if (SelectCharacterEditorTarget(context, empty->GetID()) || context.selectedEntity != mannequin.get()) return false;
	mannequin->AddComponent<CharacterCustomizationComponent>().presetPath = "assets/characters/test-look.character.json";
	if (!SelectCharacterEditorTarget(context, player->GetID()) || !SelectCharacterEditorTarget(context, mannequin->GetID())) return false;
	if (std::string(state.presetPathBuffer) != "assets/characters/test-look.character.json") return false;
	const auto removedID = mannequin->GetID();
	scene.DestroyEntity(removedID);
	if (SelectCharacterEditorTarget(context, removedID)) return false;
	std::cout << "[CharacterEditor] Character picker switching, reset and preset isolation passed" << std::endl;
#endif
	return true;
}

static bool CharacterAssetSelectionTest()
{
	CharacterAssetReference<int> reference;
	int loads = 0;
	auto load = [&](const std::string& path) -> std::shared_ptr<int>
	{
		++loads;
		return path == "missing" ? nullptr : std::make_shared<int>(42);
	};
	reference.Resolve("good", load);
	if (!reference.asset || reference.failed || loads != 1) return false;
	reference.Resolve("good", load);
	if (loads != 1) return false;
	reference.Resolve("missing", load);
	if (reference.asset || !reference.failed || loads != 2) return false;
	reference.Resolve("missing", load);
	if (loads != 2) return false;
	reference = {};
	reference.Resolve("missing", load);
	if (loads != 3 || !reference.failed) return false;
	reference.Resolve("", load);
	if (reference.asset || reference.failed || loads != 3) return false;

	CharacterCustomizationComponent original;
	original.bodyMaterial.enabled = true;
	original.bodyMaterial.materialPath = "assets/materials/body.material.json";
	original.bodyMaterial.albedoTexturePath = "assets/textures/body.png";
	BoneSocket socket;
	socket.name = "Weapon";
	socket.material.materialPath = "assets/materials/metal.material.json";
	socket.material.albedoTexturePath = "assets/textures/weapon.png";
	original.sockets.push_back(socket);
	const auto json = MyEngine::CharacterPreset::ToJson(original);
	CharacterCustomizationComponent restored;
	if (!MyEngine::CharacterPreset::FromJson(json, restored)) return false;
	if (MyEngine::CharacterPreset::ToJson(restored) != json) return false;
	if (!restored.bodyMaterial.materialAsset.attemptedPath.empty() || restored.bodyMaterial.materialAsset.asset) return false;
	const char* invalid[] = {
		R"({"bodyMaterial":{"materialPath":123}})",
		R"({"bodyMaterial":{"albedoTexturePath":"bad\u0000.png"}})",
		R"({"sockets":[{"name":"Weapon","material":{"albedoTexturePath":false}}]})"
	};
	for (const char* input : invalid)
	{
		if (MyEngine::CharacterPreset::FromJson(input, restored)) return false;
		if (MyEngine::CharacterPreset::ToJson(restored) != json) return false;
	}
	if (!MyEngine::CharacterPreset::FromJson(R"({"version":3,"bodyMaterial":{"enabled":true}})", restored)) return false;
	if (!restored.bodyMaterial.materialPath.empty() || !restored.bodyMaterial.albedoTexturePath.empty()) return false;
	std::cout << "[CharacterEditor] Asset resolution caching/retry/clear and path persistence passed" << std::endl;
	return true;
}

static bool PrefixedBodySliderTest()
{
	Scene scene;
	auto entity = scene.CreateEntity("Paused mannequin");
	auto& anim = entity->AddComponent<AnimationComponent>();
	anim.playing = false;
	anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
	MyEngine::AnimationClip clip;
	clip.name = "Idle";
	clip.durationTicks = 10;
	clip.ticksPerSecond = 1;
	anim.clips->push_back(clip);
	auto skeleton = std::make_shared<MyEngine::Skeleton>();
	const auto sourceSkeleton = BuildChainSkeleton();
	for (auto bone : sourceSkeleton->GetBones())
	{
		bone.name = "mixamorig:" + bone.name;
		skeleton->AddBone(bone);
	}
	entity->AddComponent<SkeletonComponent>().skeleton = skeleton;
	auto& customization = entity->AddComponent<CharacterCustomizationComponent>();
	customization.EnsureDefaultBodySliders();
	customization.FindSlider("Height")->value = 1;
	MyEngine::AnimationSystem animation;
	animation.Update(scene, 0);
	if (customization.FindSlider("Height")->mappings[0].boneName != "mixamorig:Hips") return false;
	if (!NearlyEqual(BoneWorldPosition(anim, *skeleton, 2), glm::vec3(0, 2.3f, 0))) return false;
	if (anim.time != 0 || anim.playing) return false;
	CharacterCustomizationComponent loaded;
	if (!MyEngine::CharacterPreset::FromJson(MyEngine::CharacterPreset::ToJson(customization), loaded)) return false;
	loaded.BindDefaultSliderBones(*skeleton);
	if (!NearlyEqual(loaded.ComputeSliderOverride("mixamorig:Hips").scaleMultiplier, glm::vec3(1.15f))) return false;
	customization.enabled = false;
	animation.Update(scene, 0);
	if (!NearlyEqual(BoneWorldPosition(anim, *skeleton, 2), glm::vec3(0, 2, 0))) return false;
	loaded.FindSlider("Height")->customMappings = true;
	loaded.FindSlider("Height")->mappings[0].boneName = "Hips";
	loaded.BindDefaultSliderBones(*skeleton);
	if (loaded.FindSlider("Height")->mappings[0].boneName != "Hips") return false;
	MyEngine::Skeleton ambiguous;
	MyEngine::Bone bone;
	bone.name = "A:Hips"; ambiguous.AddBone(bone);
	bone.name = "B:Hips"; ambiguous.AddBone(bone);
	CharacterCustomizationComponent defaults;
	defaults.EnsureDefaultBodySliders();
	defaults.BindDefaultSliderBones(ambiguous);
	if (defaults.FindSlider("Height")->mappings[0].boneName != "Hips") return false;
	std::cout << "[CharacterEditor] Paused prefixed rig slider deformation and mapping safety passed" << std::endl;
	return true;
}

static bool CharacterMaterialListTest()
{
	namespace fs = std::filesystem;
	const auto root = fs::temp_directory_path() / "myengine_character_material_list";
	fs::create_directories(root / "nested");
	{ std::ofstream file(root / "a.material.json"); file << "{}"; }
	{ std::ofstream file(root / "nested" / "b.material.json"); file << "{}"; }
	{ std::ofstream file(root / "ignored.json"); file << "{}"; }
	{ std::ofstream file(root / "ignored.material.json.bak"); file << "{}"; }
	const auto paths = MyEngine::ListCharacterMaterials(root.generic_string());
	const bool ok = paths.size() == 2 && paths[0] == (root / "a.material.json").generic_string()
		&& paths[1] == (root / "nested" / "b.material.json").generic_string()
		&& MyEngine::ListCharacterMaterials((root / "missing").generic_string()).empty();
	fs::remove_all(root);
	if (!ok) return false;
	std::cout << "[CharacterEditor] Material dropdown discovery/filtering passed" << std::endl;
	return true;
}

static bool PlayablePromotionSelectionRulesTest()
{
	auto fail = [](const char* step)
	{
		std::cout << "[CharacterEditor] Playable promotion test failed at " << step << std::endl;
		return false;
	};

	Scene scene;
	auto taggedPlayable = scene.CreateEntity("Player");
	taggedPlayable->SetTag("Player");
	taggedPlayable->AddComponent<TransformComponent>();
	taggedPlayable->AddComponent<MyEngine::CharacterControllerComponent>();
	auto otherController = scene.CreateEntity("OtherController");
	otherController->AddComponent<TransformComponent>();
	otherController->AddComponent<MyEngine::CharacterControllerComponent>();

	std::shared_ptr<Entity> restoredPlayer;
	std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> globalScripts;
	MyEngine::Editor::PlayModeRestoreContext restoreContext;
	restoreContext.scene = &scene;
	restoreContext.playerEntity = &restoredPlayer;
	restoreContext.selectedEntity = nullptr;
	restoreContext.defaultShader = nullptr;
	restoreContext.globalScripts = &globalScripts;

	MyEngine::Editor::PlayModeSession session;
	session.CaptureSnapshot(scene, {});
	session.RestoreSnapshot(restoreContext);
	if (!restoredPlayer || restoredPlayer->GetTag() != "Player") return fail("restore-tagged-player");

	// Snapshot with no Player name/tag should fall back to the first controller.
	scene.Clear();
	auto controllerOnly = scene.CreateEntity("ControllerOnly");
	controllerOnly->AddComponent<TransformComponent>();
	controllerOnly->AddComponent<MyEngine::CharacterControllerComponent>();
	auto plain = scene.CreateEntity("Plain");
	plain->AddComponent<TransformComponent>();
	session.CaptureSnapshot(scene, {});
	restoredPlayer.reset();
	session.RestoreSnapshot(restoreContext);
	if (!restoredPlayer) return fail("restore-controller-fallback");

	auto firstController = [&]() -> std::shared_ptr<Entity>
	{
		for (auto& e : scene.GetEntities())
		{
			if (e && e->HasComponent<MyEngine::CharacterControllerComponent>())
				return e;
		}
		return {};
	};
	const auto expectedController = firstController();
	if (!expectedController || restoredPlayer->GetID() != expectedController->GetID()) return fail("controller-match");

	auto camera = scene.CreateEntity("Cam");
	camera->AddComponent<TransformComponent>();
	auto& cam = camera->AddComponent<MyEngine::CameraComponent>();
	cam.isPrimary = true;
	cam.thirdPerson = true;
	cam.followTargetID = 999999;
	MyEngine::CameraSystem cameraSystem;
	cameraSystem.Update(scene, nullptr, 0.0f, 1.0f);
	if (cam.followTargetID != expectedController->GetID()) return fail("camera-follow-recovery");

	std::cout << "[CharacterEditor] Playable promotion recovery and camera fallback passed" << std::endl;
	return true;
}

bool CharacterEditorSmokeTest()
{
	if (!DeltaMatrixCompositionTest()) return false;
	if (!OverrideLayerAffectsPoseTest()) return false;
	if (!PresetRoundTripTest()) return false;
	if (!BodySliderCompositionTest()) return false;
	if (!PresetV2RoundTripTest()) return false;
	if (!SocketSystemTest()) return false;
	if (!SliderMappingEditorTest()) return false;
	if (!PresetLibraryTest()) return false;
	if (!CounterScaleChildrenTest()) return false;
	if (!CharacterMaterialTest()) return false;
	if (!CharacterPreviewCameraTest()) return false;
	if (!CharacterPickerTest()) return false;
	if (!CharacterAssetSelectionTest()) return false;
	if (!PrefixedBodySliderTest()) return false;
	if (!CharacterMaterialListTest()) return false;
	if (!PlayablePromotionSelectionRulesTest()) return false;
	return true;
}
