#pragma once

#include <cstdint>
#include <string>

// Marks a runtime-generated entity that renders a mesh attached to a bone
// socket of another (skinned) entity. These entities are owned by
// CharacterSocketSystem: they are rebuilt from the owner's
// CharacterCustomizationComponent::sockets each frame, never serialized into
// scenes, and destroyed when the owner or socket disappears.
struct SocketAttachmentComponent
{
	uint32_t ownerEntityID = 0;
	std::string socketName;

	// Mesh path that was last loaded for this attachment; used to detect
	// socket mesh changes without reloading every frame.
	std::string loadedMeshPath;
};
