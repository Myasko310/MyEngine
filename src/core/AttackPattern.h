#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace MyEngine
{
	// ============================================================
	// AttackPattern
	// ============================================================
	// Defines a reusable boss attack with timing, damage, animation,
	// and telegraph feedback. Inspired by Elden Ring's mechanical
	// depth and Dragon Ball's flashy combat choreography.
	// ============================================================

	struct AttackPattern
	{
		// Identifier
		std::string name;                        // e.g., "3-Hit Slash", "Spin Attack", "Energy Blast"
		uint32_t patternID = 0;                  // Unique ID for network sync

		// Timing (all in seconds, fixed-timestep compatible)
		float telegraphDuration = 0.5f;         // How long the attack is telegraphed before hitting
		float attackDuration = 0.8f;            // How long the attack animation plays
		float cooldownAfter = 0.3f;             // Cooldown before boss can act again

		// Damage and mechanics
		float damageDealt = 25.0f;              // Health damage to player
		float postureBreakDamage = 15.0f;       // Posture damage (stagger mechanic)
		float knockbackForce = 5.0f;            // How far player is pushed back
		glm::vec3 knockbackDirection = glm::vec3(0.0f, 0.0f, -1.0f); // Direction of knockback

		// Combat characteristics
		float criticalMultiplier = 1.0f;        // Damage multiplier if attack crits (e.g., 1.5x)
		bool canBeParried = true;               // Can player block/parry this attack?
		bool knocksDown = false;                // Does this knock player to ground?

		// Animation and visuals
		std::string animationStateName;         // Animation state to trigger (e.g., "Boss_SlashCombo")
		std::string particleEffectName;         // Particle effect during attack (e.g., "SlashEffect")
		std::string soundEffectName;            // Sound cue (e.g., "boss_slash_sfx")
		std::string telegraphVFXName;          // VFX during telegraph (e.g., "RedGlow")

		// Combo/sequence info
		uint32_t comboID = 0;                   // Links related attacks (e.g., 3 hits of same combo = comboID)
		uint32_t comboPosition = 0;             // Position in combo (0=first, 1=second, etc.)
		bool endsCombo = false;                 // Is this the last attack in a combo?

		// AI behavior hints
		float minimumPlayerDistance = 1.0f;     // Only use if player is at least this far away
		float maximumPlayerDistance = 15.0f;    // Only use if player is at most this far away
		float preferredHealthThreshold = 1.0f;  // Normalized (0-1): prefer this attack when boss health is at this %
		uint32_t phaseRequired = 1;             // Boss must be in this phase or higher to use

		// Weighting for random selection
		float selectionWeight = 1.0f;           // Higher = more likely to be chosen
	};

	// ============================================================
	// AttackPatternLibrary
	// ============================================================
	// Manages a collection of attack patterns and provides
	// pattern selection utilities for AI decision-making.
	// ============================================================
	class AttackPatternLibrary
	{
	public:
		AttackPatternLibrary();
		~AttackPatternLibrary() = default;

		// Initialize with built-in patterns
		void Initialize();

		// Pattern retrieval
		const AttackPattern* GetPattern(uint32_t patternID) const;
		const AttackPattern* GetPattern(const std::string& name) const;
		const std::vector<AttackPattern>& GetAllPatterns() const { return patterns_; }

		// Selection helpers
		// Get a random attack suitable for current combat state
		const AttackPattern* SelectRandomAttack(
			uint32_t currentPhase,
			float normalizedBossHealth,  // 0.0 (dead) to 1.0 (full)
			float playerDistance
		) const;

		// Register a custom pattern (for modding/plugin support)
		void RegisterPattern(const AttackPattern& pattern);

	private:
		std::vector<AttackPattern> patterns_;

		// Helper to create default patterns
		void RegisterDefaultPatterns();
	};
}
