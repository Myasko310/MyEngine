#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

// A single simulated particle. Lives in the component's pool; managed by ParticleSystem.
struct Particle
{
	glm::vec3 position  { 0.0f };
	glm::vec3 velocity  { 0.0f };
	glm::vec4 colorStart{ 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 colorEnd  { 1.0f, 1.0f, 1.0f, 0.0f }; // fade to transparent
	float     sizeStart { 0.1f };
	float     sizeEnd   { 0.0f };
	float     lifetime  { 1.0f }; // total lifetime in seconds
	float     age       { 0.0f }; // current age in seconds
	bool      alive     { false };
};

// Per-entity emitter settings + live particle pool.
// Attach to any entity that also has a TransformComponent.
struct ParticleEmitterComponent
{
	enum class EmissionShape
	{
		Point = 0,
		Sphere,
		Box,
		Cone
	};

	// ---------- Spawn settings ----------
	int   maxParticles  = 200;   // pool size (resize triggers pool rebuild)
	float spawnRate     = 30.0f; // particles per second
	bool  emitting      = true;  // whether the emitter is currently spawning new particles

	enum class BlendMode
	{
		Alpha = 0,
		Additive
	};

	// ---------- Emission / render shape ----------
	EmissionShape shape = EmissionShape::Point;
	BlendMode blendMode = BlendMode::Alpha;
	float shapeRadius = 0.5f;          // sphere radius or cone base radius
	glm::vec3 shapeExtents{ 0.5f };    // box half-extents
	float shapeHeight = 1.0f;          // cone height

	// ---------- Particle appearance ----------
	glm::vec4 colorStart{ 1.0f, 0.6f, 0.1f, 1.0f }; // birth color (RGBA)
	glm::vec4 colorEnd  { 1.0f, 0.1f, 0.0f, 0.0f }; // death color (fades out)
	float     sizeStart { 0.15f };
	float     sizeEnd   { 0.0f };

	// ---------- Particle motion ----------
	float     lifetime       { 1.5f };            // seconds each particle lives
	float     lifetimeVariance{ 0.5f };           // ±random added to lifetime
	glm::vec3 emitDirection  { 0.0f, 1.0f, 0.0f }; // base velocity direction
	float     emitSpeed      { 2.0f };            // base speed (m/s)
	float     emitSpeedVariance{ 0.5f };          // ±random spread on speed
	float     spreadAngle    { 25.0f };           // cone half-angle in degrees
	glm::vec3 gravity        { 0.0f, -2.0f, 0.0f }; // per-particle gravity

	// ---------- Texture ----------
	// Optional: path to a texture image (e.g. "assets/textures/particle.png").
	// Leave empty to render plain colored quads.
	std::string texturePath;

	// ---------- Runtime state (managed by ParticleSystem, not serialized) ----------
	std::vector<Particle> particles;        // live pool
	float                 spawnAccum = 0.0f; // fractional particle debt
	unsigned int          textureID  = 0;    // GL texture handle (0 = none)
	bool                  poolDirty  = true; // true → rebuild pool next Update
};
