#pragma once

#include <memory>
#include <string>

namespace MyEngine
{
	class AudioClip;
}

// Per-entity sound emitter. Supports both 2D (non-positional, e.g. music/UI)
// and 3D (positional/spatial, attenuated by distance from the listener) playback
// via the 'spatial' flag.
struct AudioSourceComponent
{
	std::shared_ptr<MyEngine::AudioClip> clip = nullptr;
	// Kept alongside the clip pointer so the source can be re-resolved
	// (e.g. after scene deserialization) without needing the clip object itself.
	std::string clipPath;

	float volume = 1.0f;
	float pitch = 1.0f;
	bool loop = false;
	bool autoPlay = false;

	// If true, this source is positioned in 3D space and attenuated by distance
	// from the listener. If false, it plays back at constant volume regardless
	// of position (suitable for music/UI sounds).
	bool spatial = true;
	float minDistance = 1.0f;
	float maxDistance = 100.0f;

	// Runtime-only state (not serialized): the underlying OpenAL source handle
	// and whether playback has been requested. Managed by AudioSystem.
	unsigned int sourceID = 0;
	bool isPlaying = false;
	bool playRequested = false;
	bool stopRequested = false;
};
