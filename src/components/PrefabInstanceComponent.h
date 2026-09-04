#pragma once

#include <cstdint>
#include <string>

struct PrefabInstanceComponent
{
	std::string sourcePrefabPath;
	std::uint32_t sourceEntityID = 0;

	// Variant metadata: when set, this instance was spawned from a variant
	// prefab that itself references another base prefab.
	bool isVariantInstance = false;
	std::string variantBasePrefabPath;
	std::uint32_t variantBaseEntityID = 0;

	// Variant delta flags relative to variantBasePrefabPath/entity.
	bool overrideName = false;
	bool overrideTag = false;
	bool overrideLayer = false;
	bool overrideTransform = false;
	bool overrideMeshRenderer = false;
	bool overrideLight = false;
	bool overrideRigidbody = false;
	bool overrideScript = false;
	bool overrideAnimation = false;
};
