#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "rendering/Skeleton.h"

// Per-bone customization applied on top of the sampled animation pose.
//
// The override is a local-space delta composed as:
//   finalLocal = animatedLocal * T(positionOffset) * R(rotationOffsetDegrees) * S(scaleMultiplier)
// before parent accumulation, so every existing clip, blend and mirror path
// keeps working untouched. This is the foundation for body shaping (M1),
// bone sockets/attachments (M2) and material slots (M3).
struct BoneOverride
{
	glm::vec3 positionOffset = glm::vec3(0.0f);
	glm::vec3 rotationOffsetDegrees = glm::vec3(0.0f);
	glm::vec3 scaleMultiplier = glm::vec3(1.0f);

	// Manual-override option: when true this bone's scale is cancelled on its
	// direct children so e.g. a thicker upper arm does not thicken the hand.
	bool counterScaleChildren = false;

	// Effective-only (computed): the accumulated scale that children must
	// inverse-apply. Populated by ComputeEffectiveOverride; not serialized.
	glm::vec3 childCounterScale = glm::vec3(1.0f);

	bool IsIdentity() const
	{
		return positionOffset == glm::vec3(0.0f)
			&& rotationOffsetDegrees == glm::vec3(0.0f)
			&& scaleMultiplier == glm::vec3(1.0f)
			&& childCounterScale == glm::vec3(1.0f);
	}

	glm::mat4 BuildDeltaMatrix() const
	{
		const glm::quat rotation = glm::quat(glm::radians(rotationOffsetDegrees));
		return glm::translate(glm::mat4(1.0f), positionOffset)
			* glm::mat4_cast(rotation)
			* glm::scale(glm::mat4(1.0f), scaleMultiplier);
	}

	// Matrix to prepend to each direct child's local transform.
	glm::mat4 BuildChildCompensationMatrix() const
	{
		if (childCounterScale == glm::vec3(1.0f))
			return glm::mat4(1.0f);
		const glm::vec3 safe = glm::max(childCounterScale, glm::vec3(1e-4f));
		return glm::scale(glm::mat4(1.0f), glm::vec3(1.0f) / safe);
	}
};

// One bone affected by a body slider. The slider value (in slider units,
// usually -1..1) is multiplied by the per-unit amounts and accumulated into
// the bone's effective override so a single slider can shape many bones.
struct BodySliderBoneMapping
{
	std::string boneName;
	glm::vec3 positionPerUnit = glm::vec3(0.0f);
	glm::vec3 rotationDegreesPerUnit = glm::vec3(0.0f);
	glm::vec3 scalePerUnit = glm::vec3(0.0f); // added to 1.0 (0.2 => 1.2x at value 1)
	bool counterScaleChildren = false;          // cancel this mapping's scale on child bones
};

// Named, animation-safe body shaping control (e.g. "Height", "Shoulder Width").
struct BodySlider
{
	std::string name;
	float value = 0.0f;
	float minValue = -1.0f;
	float maxValue = 1.0f;
	std::vector<BodySliderBoneMapping> mappings;

	// True once the user has edited this slider's mappings/range (or created
	// it by hand). Custom sliders persist their full definition in presets
	// and are left untouched by EnsureDefaultBodySliders().
	bool customMappings = false;

	bool IsDefault() const { return value == 0.0f; }
	bool HasCustomRange() const { return minValue != -1.0f || maxValue != 1.0f; }
};

namespace MyEngine { class Material; class Texture; }

template <typename T>
struct CharacterAssetReference
{
	std::string attemptedPath;
	std::shared_ptr<T> asset;
	bool failed = false;

	template <typename Loader>
	void Resolve(const std::string& path, Loader load)
	{
		if (path == attemptedPath)
			return;
		attemptedPath = path;
		asset.reset();
		failed = false;
		if (!path.empty())
		{
			asset = load(path);
			failed = !asset;
		}
	}
};

// Applied to resolved surface values, never to the shared material asset.
struct CharacterMaterialOverride
{
	bool enabled = false;
	std::string materialPath;
	std::string albedoTexturePath;
	// Transient load results; only the paths above are serialized.
	CharacterAssetReference<MyEngine::Material> materialAsset;
	CharacterAssetReference<MyEngine::Texture> textureAsset;
	void RetryAssets() { materialAsset = {}; textureAsset = {}; }
	glm::vec3 tint = glm::vec3(1.0f);
	bool overrideShininess = false;
	float shininess = 32.0f;
	bool overridePBR = false;
	float metallic = 0.0f;
	float roughness = 0.5f;
	glm::vec3 emissive = glm::vec3(0.0f);

	void Apply(glm::vec3& albedo, float& resolvedShininess, float& resolvedMetallic,
		float& resolvedRoughness, glm::vec3& resolvedEmissive) const
	{
		if (!enabled)
			return;
		albedo *= glm::clamp(tint, glm::vec3(0.0f), glm::vec3(1.0f));
		if (overrideShininess)
			resolvedShininess = glm::clamp(shininess, 1.0f, 256.0f);
		if (overridePBR)
		{
			resolvedMetallic = glm::clamp(metallic, 0.0f, 1.0f);
			resolvedRoughness = glm::clamp(roughness, 0.04f, 1.0f);
			resolvedEmissive = glm::clamp(emissive, glm::vec3(0.0f), glm::vec3(100.0f));
		}
	}
};

// A named attachment point on a bone. A static mesh is positioned at
//   entityWorld * boneModelTransform * T(positionOffset) * R(rotationOffsetDegrees) * S(scale)
// every frame by CharacterSocketSystem.
struct BoneSocket
{
	std::string name;
	std::string boneName;
	std::string meshPath;
	glm::vec3 positionOffset = glm::vec3(0.0f);
	glm::vec3 rotationOffsetDegrees = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	bool visible = true;
	CharacterMaterialOverride material;

	glm::mat4 BuildLocalMatrix() const
	{
		const glm::quat rotation = glm::quat(glm::radians(rotationOffsetDegrees));
		return glm::translate(glm::mat4(1.0f), positionOffset)
			* glm::mat4_cast(rotation)
			* glm::scale(glm::mat4(1.0f), scale);
	}
};

struct CharacterCustomizationComponent
{
	// Keyed by bone name so the data survives skeleton reloads and stays
	// retarget-friendly across characters that share a naming convention.
	std::unordered_map<std::string, BoneOverride> boneOverrides;

	// Named body-shaping sliders (M2). Ordered for stable UI/serialization.
	std::vector<BodySlider> bodySliders;

	// Bone sockets with attached meshes (M2).
	std::vector<BoneSocket> sockets;
	CharacterMaterialOverride bodyMaterial;

	// Last loaded/saved *.character.json path (empty = unsaved).
	std::string presetPath;

	// When false the override layer is bypassed (quick A/B toggle in the editor).
	bool enabled = true;

	const BoneOverride* FindOverride(const std::string& boneName) const
	{
		const auto it = boneOverrides.find(boneName);
		return it == boneOverrides.end() ? nullptr : &it->second;
	}

	BoneOverride& GetOrCreateOverride(const std::string& boneName)
	{
		return boneOverrides[boneName];
	}

	void ResetOverride(const std::string& boneName)
	{
		boneOverrides.erase(boneName);
	}

	void ResetAll()
	{
		boneOverrides.clear();
		for (auto& slider : bodySliders)
			slider.value = 0.0f;
	}

	bool HasAnyOverride() const
	{
		for (const auto& [name, value] : boneOverrides)
		{
			if (!value.IsIdentity())
				return true;
		}
		for (const auto& slider : bodySliders)
		{
			if (!slider.IsDefault())
				return true;
		}
		return false;
	}

	bool HasAnySliderInfluence() const
	{
		for (const auto& slider : bodySliders)
		{
			if (!slider.IsDefault() && !slider.mappings.empty())
				return true;
		}
		return false;
	}

	BodySlider* FindSlider(const std::string& name)
	{
		for (auto& slider : bodySliders)
		{
			if (slider.name == name)
				return &slider;
		}
		return nullptr;
	}

	bool RemoveSlider(const std::string& name)
	{
		for (auto it = bodySliders.begin(); it != bodySliders.end(); ++it)
		{
			if (it->name == name)
			{
				bodySliders.erase(it);
				return true;
			}
		}
		return false;
	}

	// Creates an empty, user-defined slider. Returns null if the name is
	// empty or already taken.
	BodySlider* AddCustomSlider(const std::string& name)
	{
		if (name.empty() || FindSlider(name))
			return nullptr;
		BodySlider slider;
		slider.name = name;
		slider.customMappings = true;
		bodySliders.push_back(std::move(slider));
		return &bodySliders.back();
	}

	// Restores the rig-default mappings/range for one slider (if it is a
	// default slider) and clears its custom flag. Returns false when the
	// slider has no default definition.
	bool ResetSliderToDefault(const std::string& name)
	{
		BodySlider* slider = FindSlider(name);
		if (!slider)
			return false;

		CharacterCustomizationComponent defaults;
		defaults.EnsureDefaultBodySliders();
		const BodySlider* def = defaults.FindSlider(name);
		if (!def)
			return false;

		slider->mappings = def->mappings;
		slider->minValue = def->minValue;
		slider->maxValue = def->maxValue;
		slider->customMappings = false;
		return true;
	}

	BoneSocket* FindSocket(const std::string& name)
	{
		for (auto& socket : sockets)
		{
			if (socket.name == name)
				return &socket;
		}
		return nullptr;
	}

	// Resolve only unmodified defaults; explicit custom mappings stay exact.
	void BindDefaultSliderBones(const MyEngine::Skeleton& skeleton)
	{
		auto bareName = [](const std::string& name)
		{
			const auto separator = name.find_last_of(':');
			return name.substr(separator == std::string::npos ? 0 : separator + 1);
		};
		for (auto& slider : bodySliders)
		{
			if (slider.customMappings) continue;
			for (auto& mapping : slider.mappings)
			{
				if (skeleton.GetBoneIndex(mapping.boneName) >= 0) continue;
				const auto name = bareName(mapping.boneName);
				const MyEngine::Bone* match = nullptr;
				for (const auto& bone : skeleton.GetBones())
				{
					if (bareName(bone.name) != name) continue;
					if (match) { match = nullptr; break; }
					match = &bone;
				}
				if (match) mapping.boneName = match->name;
			}
		}
	}

	// Slider-only contribution for a bone (no manual override).
	BoneOverride ComputeSliderOverride(const std::string& boneName) const
	{
		BoneOverride effective;
		for (const auto& slider : bodySliders)
		{
			if (slider.IsDefault())
				continue;
			for (const auto& mapping : slider.mappings)
			{
				if (mapping.boneName != boneName)
					continue;
				effective.positionOffset += mapping.positionPerUnit * slider.value;
				effective.rotationOffsetDegrees += mapping.rotationDegreesPerUnit * slider.value;
				const glm::vec3 scale = glm::vec3(1.0f) + mapping.scalePerUnit * slider.value;
				effective.scaleMultiplier *= scale;
				if (mapping.counterScaleChildren)
					effective.childCounterScale *= scale;
			}
		}
		return effective;
	}

	// Combines slider contributions and the manual per-bone override into the
	// delta actually applied by AnimationSystem. Slider offsets are summed
	// (positions/rotations additive, scales multiplicative) and then the manual
	// override is composed on top, so hand-posing stays independent of sliders.
	BoneOverride ComputeEffectiveOverride(const std::string& boneName) const
	{
		BoneOverride effective = ComputeSliderOverride(boneName);
		if (const BoneOverride* manual = FindOverride(boneName))
		{
			effective.positionOffset += manual->positionOffset;
			effective.rotationOffsetDegrees += manual->rotationOffsetDegrees;
			effective.scaleMultiplier *= manual->scaleMultiplier;
			if (manual->counterScaleChildren)
				effective.childCounterScale *= manual->scaleMultiplier;
		}
		return effective;
	}

	// Installs the default slider set for Mixamo-style humanoid rigs (the
	// Akaza player). Existing slider values with matching names are preserved.
	void EnsureDefaultBodySliders()
	{
		auto add = [this](const char* name, std::vector<BodySliderBoneMapping> mappings)
		{
			if (BodySlider* existing = FindSlider(name))
			{
				if (!existing->customMappings)
					existing->mappings = std::move(mappings);
				return;
			}
			BodySlider slider;
			slider.name = name;
			slider.mappings = std::move(mappings);
			bodySliders.push_back(std::move(slider));
		};

		auto scaleMap = [](const char* bone, glm::vec3 scalePerUnit, bool counterScaleChildren = false)
		{
			BodySliderBoneMapping m;
			m.boneName = bone;
			m.scalePerUnit = scalePerUnit;
			m.counterScaleChildren = counterScaleChildren;
			return m;
		};

		add("Height", { scaleMap("Hips", glm::vec3(0.15f)) });
		add("Torso Length", { scaleMap("Spine", glm::vec3(0.0f, 0.2f, 0.0f)) });
		add("Torso Bulk", { scaleMap("Spine1", glm::vec3(0.2f, 0.0f, 0.2f)), scaleMap("Spine2", glm::vec3(0.2f, 0.0f, 0.2f)) });
		add("Shoulder Width", { scaleMap("LeftShoulder", glm::vec3(0.25f, 0.0f, 0.0f)), scaleMap("RightShoulder", glm::vec3(0.25f, 0.0f, 0.0f)) });
		add("Arm Length", { scaleMap("LeftArm", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("RightArm", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("LeftForeArm", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("RightForeArm", glm::vec3(0.2f, 0.0f, 0.0f)) });
		add("Arm Thickness", { scaleMap("LeftArm", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("RightArm", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("LeftForeArm", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("RightForeArm", glm::vec3(0.0f, 0.3f, 0.3f), true) });
		add("Leg Length", { scaleMap("LeftUpLeg", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("RightUpLeg", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("LeftLeg", glm::vec3(0.2f, 0.0f, 0.0f)), scaleMap("RightLeg", glm::vec3(0.2f, 0.0f, 0.0f)) });
		add("Leg Thickness", { scaleMap("LeftUpLeg", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("RightUpLeg", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("LeftLeg", glm::vec3(0.0f, 0.3f, 0.3f), true), scaleMap("RightLeg", glm::vec3(0.0f, 0.3f, 0.3f), true) });
		add("Neck Length", { scaleMap("Neck", glm::vec3(0.0f, 0.3f, 0.0f)) });
		add("Head Size", { scaleMap("Head", glm::vec3(0.2f)) });
	}
};
