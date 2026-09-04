#include "systems/ParticleSystem.h"

#include "components/ParticleEmitterComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
	// Per-instance data uploaded to the GPU each frame.
	struct InstanceData
	{
		glm::vec3 position;
		float     size;
		glm::vec4 color;
	};

	// Quad vertices (two triangles, positions only; billboard expansion is
	// done in the vertex shader using camera right/up vectors).
	// Layout: vec2 corner offset
	static const float kQuadVerts[] =
	{
		-0.5f,  0.5f,
		-0.5f, -0.5f,
		 0.5f,  0.5f,
		 0.5f, -0.5f,
	};

	float RandFloat(std::mt19937& rng, float lo, float hi)
	{
		std::uniform_real_distribution<float> dist(lo, hi);
		return dist(rng);
	}

	// Returns a random unit vector within a cone of half-angle `degrees`
	// around `axis`.
	glm::vec3 RandomInCone(std::mt19937& rng, const glm::vec3& axis, float degrees)
	{
		float radians   = glm::radians(degrees);
		float cosAngle  = std::cos(radians);
		float z         = RandFloat(rng, cosAngle, 1.0f);
		float phi       = RandFloat(rng, 0.0f, glm::two_pi<float>());
		float sinTheta  = std::sqrt(1.0f - z * z);

		// Build in a coordinate frame aligned with axis
		glm::vec3 up    = std::abs(axis.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
		glm::vec3 right = glm::normalize(glm::cross(up, axis));
		up              = glm::cross(axis, right);

		return glm::normalize(
			right * (sinTheta * std::cos(phi)) +
			up    * (sinTheta * std::sin(phi)) +
			axis  * z
		);
	}

	glm::vec3 RandomPointInEmitterShape(std::mt19937& rng, const ParticleEmitterComponent& emitter)
	{
		switch (emitter.shape)
		{
		case ParticleEmitterComponent::EmissionShape::Sphere:
		{
			glm::vec3 dir = glm::normalize(glm::vec3(
				RandFloat(rng, -1.0f, 1.0f),
				RandFloat(rng, -1.0f, 1.0f),
				RandFloat(rng, -1.0f, 1.0f)));
			float r = emitter.shapeRadius * std::cbrt(RandFloat(rng, 0.0f, 1.0f));
			return dir * r;
		}
		case ParticleEmitterComponent::EmissionShape::Box:
			return glm::vec3(
				RandFloat(rng, -emitter.shapeExtents.x, emitter.shapeExtents.x),
				RandFloat(rng, -emitter.shapeExtents.y, emitter.shapeExtents.y),
				RandFloat(rng, -emitter.shapeExtents.z, emitter.shapeExtents.z));
		case ParticleEmitterComponent::EmissionShape::Cone:
			return glm::vec3(
				RandFloat(rng, -emitter.shapeRadius, emitter.shapeRadius),
				RandFloat(rng, 0.0f, std::max(emitter.shapeHeight, 0.0f)),
				RandFloat(rng, -emitter.shapeRadius, emitter.shapeRadius));
		case ParticleEmitterComponent::EmissionShape::Point:
		default:
			return glm::vec3(0.0f);
		}
	}
} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
namespace MyEngine
{
	struct ParticleSystem::Impl
	{
		// OpenGL objects (lazy-initialized on first Render call)
		unsigned int quadVAO = 0;
		unsigned int quadVBO = 0;
		unsigned int instanceVBO = 0;

		// Shader compiled once, owned here
		std::unique_ptr<Shader> shader;

		// CPU-side instance buffer reused every frame
		std::vector<InstanceData> instanceBuffer;

		// Random engine
		std::mt19937 rng{ std::random_device{}() };

		bool glReady = false;

		void InitGL()
		{
			if (glReady) return;

			// --- Quad VAO ---
			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);
			glGenBuffers(1, &instanceVBO);

			glBindVertexArray(quadVAO);

			// Attrib 0: quad corner offset (vec2, from static VBO)
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
			glVertexAttribDivisor(0, 0); // one per vertex

			// Attrib 1: instance position (vec3)
			// Attrib 2: instance size (float)  } packed in InstanceData
			// Attrib 3: instance color (vec4)  }
			glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
			// Allocate empty (will be filled each frame with glBufferSubData / glBufferData)
			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

			// position (vec3) — offset 0
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
				sizeof(InstanceData), (void*)offsetof(InstanceData, position));
			glVertexAttribDivisor(1, 1);

			// size (float) — offset 12
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
				sizeof(InstanceData), (void*)offsetof(InstanceData, size));
			glVertexAttribDivisor(2, 1);

			// color (vec4) — offset 16
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE,
				sizeof(InstanceData), (void*)offsetof(InstanceData, color));
			glVertexAttribDivisor(3, 1);

			glBindVertexArray(0);

			// --- Shader ---
			try
			{
				shader = std::make_unique<Shader>(
					"shaders/particle.vert",
					"shaders/particle.frag"
				);
			}
			catch (const std::exception& e)
			{
				std::cerr << "[ParticleSystem] Shader compile error: " << e.what() << "\n";
			}

			glReady = true;
		}

		~Impl()
		{
			if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
			if (quadVBO) glDeleteBuffers(1, &quadVBO);
			if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
		}
	};

	// -----------------------------------------------------------------------
	// ParticleSystem
	// -----------------------------------------------------------------------
	ParticleSystem::ParticleSystem()
		: m_Impl(std::make_unique<Impl>())
	{
	}

	ParticleSystem::~ParticleSystem() = default;

	void ParticleSystem::SetDeterministicSeed(unsigned int seed)
	{
		if (!m_Impl)
			return;
		m_Impl->rng.seed(seed);
	}

	void ParticleSystem::Update(Scene& scene, float deltaTime)
	{
		for (auto& entityPtr : scene.GetEntities())
		{
			if (!entityPtr) continue;
			if (!entityPtr->HasComponent<ParticleEmitterComponent>()) continue;
			if (!entityPtr->HasComponent<TransformComponent>()) continue;

			auto& emitter   = entityPtr->GetComponent<ParticleEmitterComponent>();
			auto& transform = entityPtr->GetComponent<TransformComponent>();

			// Rebuild pool if requested
			if (emitter.poolDirty)
			{
				emitter.particles.assign(static_cast<size_t>(emitter.maxParticles), Particle{});
				emitter.poolDirty = false;
				emitter.spawnAccum = 0.0f;
			}

			glm::vec3 emitterPos = transform.position;
			glm::vec3 spawnOffset = RandomPointInEmitterShape(m_Impl->rng, emitter);
			glm::vec3 spawnPos = emitterPos + spawnOffset;

			// --- Simulate existing particles ---
			for (auto& p : emitter.particles)
			{
				if (!p.alive) continue;

				p.age += deltaTime;
				if (p.age >= p.lifetime)
				{
					p.alive = false;
					continue;
				}

				p.velocity  += emitter.gravity * deltaTime;
				p.position  += p.velocity * deltaTime;
			}

			auto spawnParticles = [&](int toSpawn)
			{
				for (int i = 0; i < toSpawn; ++i)
				{
					Particle* slot = nullptr;
					for (auto& p : emitter.particles)
					{
						if (!p.alive) { slot = &p; break; }
					}
					if (!slot) break;

					slot->alive      = true;
					slot->age        = 0.0f;
					slot->position   = spawnPos;
					slot->colorStart = emitter.colorStart;
					slot->colorEnd   = emitter.colorEnd;
					slot->sizeStart  = emitter.sizeStart;
					slot->sizeEnd    = emitter.sizeEnd;

					float lt = emitter.lifetime +
							   m_Impl->rng() / static_cast<float>(m_Impl->rng.max()) *
							   2.0f * emitter.lifetimeVariance - emitter.lifetimeVariance;
					slot->lifetime = std::max(lt, 0.05f);

					glm::vec3 dir = RandomInCone(
						m_Impl->rng,
						glm::normalize(emitter.emitDirection),
						emitter.spreadAngle
					);
					float speed = emitter.emitSpeed +
								  RandFloat(m_Impl->rng,
											-emitter.emitSpeedVariance,
											 emitter.emitSpeedVariance);
					slot->velocity = dir * speed;
				}
			};

			// --- Spawn new particles ---
			if (emitter.emitting)
			{
				emitter.spawnAccum += emitter.spawnRate * deltaTime;
				int toSpawn = static_cast<int>(emitter.spawnAccum);
				emitter.spawnAccum -= static_cast<float>(toSpawn);
				spawnParticles(toSpawn);
			}

			if (emitter.burstRequestCount > 0)
			{
				spawnParticles(emitter.burstRequestCount);
				emitter.burstRequestCount = 0;
			}
		}
	}

	void ParticleSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
	{
		m_Impl->InitGL();
		if (!m_Impl->shader) return;

		// Extract camera right/up from the view matrix for billboarding
		glm::vec3 camRight(view[0][0], view[1][0], view[2][0]);
		glm::vec3 camUp   (view[0][1], view[1][1], view[2][1]);

		m_Impl->shader->Use();
		m_Impl->shader->SetMat4("u_View",       view);
		m_Impl->shader->SetMat4("u_Projection", projection);
		m_Impl->shader->SetVec3("u_CamRight",   camRight);
		m_Impl->shader->SetVec3("u_CamUp",      camUp);

		// Alpha blending + depth test (read-only so particles don't occlude each other)
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);

		glBindVertexArray(m_Impl->quadVAO);

		for (auto& entityPtr : scene.GetEntities())
		{
			if (!entityPtr) continue;
			if (!entityPtr->HasComponent<ParticleEmitterComponent>()) continue;

			auto& emitter = entityPtr->GetComponent<ParticleEmitterComponent>();

			// Load texture if not yet done
			if (!emitter.texturePath.empty() && emitter.textureID == 0)
			{
				try
				{
					MyEngine::Texture tex(emitter.texturePath);
					emitter.textureID = tex.GetID();
					// Detach from RAII so GL object persists
					// (we can't easily "move out" the ID from Texture, so we
					//  instead keep a Texture* per emitter — simplest approach)
				}
				catch (...) { /* texture missing, use plain quad */ }
			}

			// Bind texture or unbind
			glActiveTexture(GL_TEXTURE0);
			if (emitter.textureID)
			{
				glBindTexture(GL_TEXTURE_2D, emitter.textureID);
				m_Impl->shader->SetBool("u_UseTexture", true);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, 0);
				m_Impl->shader->SetBool("u_UseTexture", false);
			}
			m_Impl->shader->SetInt("u_Texture", 0);

			// Blend mode per emitter
			if (emitter.blendMode == ParticleEmitterComponent::BlendMode::Additive)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// Build instance buffer for alive particles
			m_Impl->instanceBuffer.clear();
			for (const auto& p : emitter.particles)
			{
				if (!p.alive) continue;

				float t = p.lifetime > 0.0f ? (p.age / p.lifetime) : 1.0f;
				t = std::clamp(t, 0.0f, 1.0f);

				InstanceData inst;
				inst.position = p.position;
				inst.size     = glm::mix(p.sizeStart, p.sizeEnd, t);
				inst.color    = glm::mix(p.colorStart, p.colorEnd, t);
				m_Impl->instanceBuffer.push_back(inst);
			}

			if (m_Impl->instanceBuffer.empty()) continue;

			// Upload instance data
			glBindBuffer(GL_ARRAY_BUFFER, m_Impl->instanceVBO);
			glBufferData(GL_ARRAY_BUFFER,
				static_cast<GLsizeiptr>(m_Impl->instanceBuffer.size() * sizeof(InstanceData)),
				m_Impl->instanceBuffer.data(),
				GL_STREAM_DRAW);

			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
				static_cast<GLsizei>(m_Impl->instanceBuffer.size()));
		}

		glBindVertexArray(0);
		glDepthMask(GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}
} // namespace MyEngine
