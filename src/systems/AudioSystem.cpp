#include "systems/AudioSystem.h"

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"
#include "components/CameraComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/AudioListenerComponent.h"
#include "audio/AudioEngine.h"
#include "audio/AudioClip.h"

#include <AL/al.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>

namespace MyEngine
{
	namespace
	{
		glm::vec3 ComputeForward(const std::shared_ptr<::Entity>& entity, const TransformComponent& transform)
		{
			if (entity->HasComponent<CameraComponent>())
			{
				const auto& cam = entity->GetComponent<CameraComponent>();
				float yawRad = glm::radians(cam.yaw);
				float pitchRad = glm::radians(cam.pitch);
				glm::vec3 forward;
				forward.x = cosf(yawRad) * cosf(pitchRad);
				forward.y = sinf(pitchRad);
				forward.z = sinf(yawRad) * cosf(pitchRad);
				return glm::normalize(forward);
			}

			// Fall back to rotating the -Z axis by the transform's rotation.
			glm::quat rotationQuat = glm::quat(transform.rotation);
			return glm::normalize(rotationQuat * glm::vec3(0.0f, 0.0f, -1.0f));
		}
	}

	AudioSystem::AudioSystem() = default;
	AudioSystem::~AudioSystem() = default;

	void AudioSystem::Update(Scene& scene, float /*deltaTime*/)
	{
		if (!AudioEngine::IsInitialized())
			return;

		// --- Listener ---
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;

			if (!entity->HasComponent<AudioListenerComponent>() || !entity->HasComponent<TransformComponent>())
				continue;

			auto& listener = entity->GetComponent<AudioListenerComponent>();
			if (!listener.isPrimary)
				continue;

			const auto& transform = entity->GetComponent<TransformComponent>();
			glm::vec3 forward = ComputeForward(entity, transform);
			glm::vec3 up(0.0f, 1.0f, 0.0f);

			AudioEngine::SetListenerPosition(transform.position);
			AudioEngine::SetListenerOrientation(forward, up);
			AudioEngine::SetListenerGain(listener.gain);
			break; // only the first primary listener is used
		}

		// --- Sources ---
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;

			if (!entity->HasComponent<AudioSourceComponent>())
				continue;

			auto& source = entity->GetComponent<AudioSourceComponent>();

			if (!source.clip || !source.clip->IsValid())
				continue;

			// Lazily create the OpenAL source the first time this component is seen.
			if (source.sourceID == 0)
			{
				alGenSources(1, &source.sourceID);
				alSourcei(source.sourceID, AL_BUFFER, static_cast<ALint>(source.clip->GetBufferID()));
				alSourcei(source.sourceID, AL_LOOPING, source.loop ? AL_TRUE : AL_FALSE);
				alSourcef(source.sourceID, AL_GAIN, source.volume);
				alSourcef(source.sourceID, AL_PITCH, source.pitch);
				alSourcei(source.sourceID, AL_SOURCE_RELATIVE, source.spatial ? AL_FALSE : AL_TRUE);
				alSourcef(source.sourceID, AL_REFERENCE_DISTANCE, source.minDistance);
				alSourcef(source.sourceID, AL_MAX_DISTANCE, source.maxDistance);

				if (source.autoPlay)
					source.playRequested = true;
			}

			// Keep dynamic parameters in sync every frame (cheap to set).
			alSourcef(source.sourceID, AL_GAIN, source.volume);
			alSourcef(source.sourceID, AL_PITCH, source.pitch);
			alSourcei(source.sourceID, AL_LOOPING, source.loop ? AL_TRUE : AL_FALSE);

			if (source.spatial && entity->HasComponent<TransformComponent>())
			{
				const auto& transform = entity->GetComponent<TransformComponent>();
				alSource3f(source.sourceID, AL_POSITION, transform.position.x, transform.position.y, transform.position.z);
			}

			if (source.playRequested)
			{
				alSourcePlay(source.sourceID);
				source.isPlaying = true;
				source.playRequested = false;
			}

			if (source.stopRequested)
			{
				alSourceStop(source.sourceID);
				source.isPlaying = false;
				source.stopRequested = false;
			}

			// Reflect actual OpenAL playback state (e.g. a non-looping clip finishing on its own).
			if (source.isPlaying)
			{
				ALint state = AL_STOPPED;
				alGetSourcei(source.sourceID, AL_SOURCE_STATE, &state);
				if (state != AL_PLAYING)
					source.isPlaying = false;
			}
		}
	}

	void AudioSystem::ReleaseAll(Scene& scene)
	{
		for (const auto& entity : scene.GetEntities())
		{
			if (!entity)
				continue;

			if (!entity->HasComponent<AudioSourceComponent>())
				continue;

			auto& source = entity->GetComponent<AudioSourceComponent>();
			if (source.sourceID != 0)
			{
				alSourceStop(source.sourceID);
				alDeleteSources(1, &source.sourceID);
				source.sourceID = 0;
				source.isPlaying = false;
			}
		}
	}
}
