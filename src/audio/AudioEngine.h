#pragma once

#include <glm/glm.hpp>

namespace MyEngine
{
	// Owns the OpenAL device/context lifecycle. This is the low-level audio
	// backend; ECS-facing playback logic lives in AudioSystem.
	class AudioEngine
	{
	public:
		static bool Init();
		static void Shutdown();
		static bool IsInitialized();

		// Listener (usually driven by AudioSystem from an AudioListenerComponent)
		static void SetListenerPosition(const glm::vec3& position);
		static void SetListenerVelocity(const glm::vec3& velocity);
		static void SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
		static void SetListenerGain(float gain);

		// Simple master audio controls (applied on top of listener gain)
		static void SetMasterVolume(float volume);
		static float GetMasterVolume();
		static void SetMuted(bool muted);
		static bool IsMuted();

	private:
		static bool s_Initialized;
	};
}
