#include "audio/AudioEngine.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>

namespace MyEngine
{
	namespace
	{
		ALCdevice* s_Device = nullptr;
		ALCcontext* s_Context = nullptr;
		float s_MasterVolume = 1.0f;
		bool  s_Muted = false;
		std::unordered_map<std::string, float> s_BusVolumes;
	}

	bool AudioEngine::s_Initialized = false;

	bool AudioEngine::Init()
	{
		if (s_Initialized)
			return true;

		s_Device = alcOpenDevice(nullptr); // default device
		if (!s_Device)
		{
			std::cerr << "[AudioEngine] Failed to open default OpenAL device." << std::endl;
			return false;
		}

		s_Context = alcCreateContext(s_Device, nullptr);
		if (!s_Context || alcMakeContextCurrent(s_Context) == ALC_FALSE)
		{
			std::cerr << "[AudioEngine] Failed to create/activate OpenAL context." << std::endl;
			if (s_Context)
			{
				alcDestroyContext(s_Context);
				s_Context = nullptr;
			}
			alcCloseDevice(s_Device);
			s_Device = nullptr;
			return false;
		}

		// Reasonable defaults
		s_BusVolumes.clear();
		s_BusVolumes["Master"] = 1.0f;
		alListenerf(AL_GAIN, 1.0f);
		alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
		alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
		float orientation[6] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
		alListenerfv(AL_ORIENTATION, orientation);

		s_Initialized = true;
		std::cout << "[AudioEngine] Initialized OpenAL device: " << alcGetString(s_Device, ALC_DEVICE_SPECIFIER) << std::endl;
		return true;
	}

	void AudioEngine::Shutdown()
	{
		if (!s_Initialized)
			return;

		alcMakeContextCurrent(nullptr);

		if (s_Context)
		{
			alcDestroyContext(s_Context);
			s_Context = nullptr;
		}

		if (s_Device)
		{
			alcCloseDevice(s_Device);
			s_Device = nullptr;
		}

		s_Initialized = false;
	}

	bool AudioEngine::IsInitialized()
	{
		return s_Initialized;
	}

	void AudioEngine::SetListenerPosition(const glm::vec3& position)
	{
		if (!s_Initialized)
			return;
		alListener3f(AL_POSITION, position.x, position.y, position.z);
	}

	void AudioEngine::SetListenerVelocity(const glm::vec3& velocity)
	{
		if (!s_Initialized)
			return;
		alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
	}

	void AudioEngine::SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up)
	{
		if (!s_Initialized)
			return;
		float orientation[6] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
		alListenerfv(AL_ORIENTATION, orientation);
	}

	void AudioEngine::SetListenerGain(float gain)
	{
		if (!s_Initialized)
			return;
		float effectiveGain = s_Muted ? 0.0f : gain * s_MasterVolume;
		alListenerf(AL_GAIN, effectiveGain);
	}

	void AudioEngine::SetMasterVolume(float volume)
	{
		s_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
		if (s_Initialized)
			alGetError(); // clear any stale error before setting gain
	}

	float AudioEngine::GetMasterVolume()
	{
		return s_MasterVolume;
	}

	void AudioEngine::SetMuted(bool muted)
	{
		s_Muted = muted;
	}

	bool AudioEngine::IsMuted()
	{
		return s_Muted;
	}

	void AudioEngine::SetBusVolume(const char* busName, float volume)
	{
		const std::string key = (busName && busName[0] != '\0') ? busName : "Master";
		s_BusVolumes[key] = std::clamp(volume, 0.0f, 1.0f);
	}

	float AudioEngine::GetBusVolume(const char* busName)
	{
		const std::string key = (busName && busName[0] != '\0') ? busName : "Master";
		auto it = s_BusVolumes.find(key);
		if (it != s_BusVolumes.end())
			return it->second;

		if (key == "Master")
			return 1.0f;

		return GetBusVolume("Master");
	}
}
