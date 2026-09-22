#include "core/AttackPattern.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace MyEngine
{
	AttackPatternLibrary::AttackPatternLibrary()
	{
		srand(static_cast<unsigned int>(time(nullptr)));
	}

	void AttackPatternLibrary::Initialize()
	{
		RegisterDefaultPatterns();
	}

	const AttackPattern* AttackPatternLibrary::GetPattern(uint32_t patternID) const
	{
		for (const auto& pattern : patterns_)
		{
			if (pattern.patternID == patternID)
				return &pattern;
		}
		return nullptr;
	}

	const AttackPattern* AttackPatternLibrary::GetPattern(const std::string& name) const
	{
		for (const auto& pattern : patterns_)
		{
			if (pattern.name == name)
				return &pattern;
		}
		return nullptr;
	}

	const AttackPattern* AttackPatternLibrary::SelectRandomAttack(
		uint32_t currentPhase,
		float normalizedBossHealth,
		float playerDistance) const
	{
		// Filter patterns: phase requirement, distance, and viability
		std::vector<const AttackPattern*> validPatterns;
		std::vector<float> weights;

		for (const auto& pattern : patterns_)
		{
			// Phase filter
			if (pattern.phaseRequired > currentPhase)
				continue;

			// Distance filter
			if (playerDistance < pattern.minimumPlayerDistance || playerDistance > pattern.maximumPlayerDistance)
				continue;

			// Base weight
			float weight = pattern.selectionWeight;

			// Adjust weight based on health threshold preference
			float healthDeviation = std::abs(normalizedBossHealth - pattern.preferredHealthThreshold);
			float healthBonus = std::max(0.0f, 1.0f - healthDeviation);  // Peak weight at preferred health
			weight *= (1.0f + healthBonus * 0.5f);

			// Higher phase = slightly more aggressive weighting
			weight *= (1.0f + (currentPhase - 1) * 0.2f);

			validPatterns.push_back(&pattern);
			weights.push_back(weight);
		}

		if (validPatterns.empty())
			return nullptr;

		// Weighted random selection
		float totalWeight = 0.0f;
		for (float w : weights)
			totalWeight += w;

		float selector = (rand() / (float)RAND_MAX) * totalWeight;
		float accumulator = 0.0f;

		for (size_t i = 0; i < validPatterns.size(); ++i)
		{
			accumulator += weights[i];
			if (selector <= accumulator)
				return validPatterns[i];
		}

		return validPatterns.back();
	}

	void AttackPatternLibrary::RegisterPattern(const AttackPattern& pattern)
	{
		patterns_.push_back(pattern);
	}

	void AttackPatternLibrary::RegisterDefaultPatterns()
	{
		// ============================================================
		// PHASE 1: Single strikes, moderate speed
		// ============================================================

		// Pattern 1: 3-Hit Combo (Elden Ring inspired)
		AttackPattern slashCombo;
		slashCombo.name = "3-Hit Slash Combo";
		slashCombo.patternID = 1;
		slashCombo.telegraphDuration = 0.4f;
		slashCombo.attackDuration = 0.6f;
		slashCombo.cooldownAfter = 0.2f;
		slashCombo.damageDealt = 20.0f;
		slashCombo.postureBreakDamage = 10.0f;
		slashCombo.knockbackForce = 3.0f;
		slashCombo.canBeParried = true;
		slashCombo.animationStateName = "Boss_SlashCombo";
		slashCombo.particleEffectName = "SlashTrail";
		slashCombo.soundEffectName = "boss_slash";
		slashCombo.telegraphVFXName = "RedWarning_Weak";
		slashCombo.comboID = 1;
		slashCombo.comboPosition = 0;
		slashCombo.minimumPlayerDistance = 2.0f;
		slashCombo.maximumPlayerDistance = 6.0f;
		slashCombo.preferredHealthThreshold = 1.0f;  // Prefer early phase
		slashCombo.phaseRequired = 1;
		slashCombo.selectionWeight = 1.5f;
		RegisterPattern(slashCombo);

		// Pattern 2: Spin Attack (Dragon Ball inspired)
		AttackPattern spinAttack;
		spinAttack.name = "Spin Attack";
		spinAttack.patternID = 2;
		spinAttack.telegraphDuration = 0.5f;
		spinAttack.attackDuration = 1.2f;
		spinAttack.cooldownAfter = 0.4f;
		spinAttack.damageDealt = 30.0f;
		spinAttack.postureBreakDamage = 20.0f;
		spinAttack.knockbackForce = 5.0f;
		spinAttack.canBeParried = true;
		spinAttack.animationStateName = "Boss_Spin";
		spinAttack.particleEffectName = "SpinEffect";
		spinAttack.soundEffectName = "boss_spin";
		spinAttack.telegraphVFXName = "RedGlow_Medium";
		spinAttack.minimumPlayerDistance = 1.5f;
		spinAttack.maximumPlayerDistance = 8.0f;
		spinAttack.preferredHealthThreshold = 0.8f;
		spinAttack.phaseRequired = 1;
		spinAttack.selectionWeight = 1.2f;
		RegisterPattern(spinAttack);

		// Pattern 3: Charged Energy Blast (Dragon Ball inspired)
		AttackPattern energyBlast;
		energyBlast.name = "Energy Blast";
		energyBlast.patternID = 3;
		energyBlast.telegraphDuration = 1.0f;  // Longer telegraph = time to dodge
		energyBlast.attackDuration = 0.4f;
		energyBlast.cooldownAfter = 0.5f;
		energyBlast.damageDealt = 25.0f;
		energyBlast.postureBreakDamage = 5.0f;  // Blasts don't posture-break as much
		energyBlast.knockbackForce = 8.0f;  // High knockback instead
		energyBlast.canBeParried = false;  // Can't parry energy blasts
		energyBlast.animationStateName = "Boss_BlastCharge";
		energyBlast.particleEffectName = "EnergyCharge";
		energyBlast.soundEffectName = "boss_energy_blast";
		energyBlast.telegraphVFXName = "YellowGlow_Strong";
		energyBlast.minimumPlayerDistance = 5.0f;  // Only at range
		energyBlast.maximumPlayerDistance = 15.0f;
		energyBlast.preferredHealthThreshold = 0.7f;
		energyBlast.phaseRequired = 1;
		energyBlast.selectionWeight = 1.0f;
		RegisterPattern(energyBlast);

		// ============================================================
		// PHASE 2: Faster attacks, combos unlock
		// ============================================================

		// Pattern 4: Double Slash
		AttackPattern doubleSlash;
		doubleSlash.name = "Double Slash";
		doubleSlash.patternID = 4;
		doubleSlash.telegraphDuration = 0.3f;
		doubleSlash.attackDuration = 0.5f;
		doubleSlash.cooldownAfter = 0.15f;
		doubleSlash.damageDealt = 18.0f;
		doubleSlash.postureBreakDamage = 12.0f;
		doubleSlash.knockbackForce = 3.5f;
		doubleSlash.canBeParried = true;
		doubleSlash.animationStateName = "Boss_DoubleSlash";
		doubleSlash.particleEffectName = "SlashTrail_Fast";
		doubleSlash.soundEffectName = "boss_double_slash";
		doubleSlash.telegraphVFXName = "RedFlash_Fast";
		doubleSlash.minimumPlayerDistance = 2.0f;
		doubleSlash.maximumPlayerDistance = 6.0f;
		doubleSlash.preferredHealthThreshold = 0.5f;
		doubleSlash.phaseRequired = 2;
		doubleSlash.selectionWeight = 2.0f;  // More likely in phase 2
		RegisterPattern(doubleSlash);

		// Pattern 5: Leaping Slam (AOE-like, ground impact)
		AttackPattern leapingSlam;
		leapingSlam.name = "Leaping Slam";
		leapingSlam.patternID = 5;
		leapingSlam.telegraphDuration = 0.6f;
		leapingSlam.attackDuration = 0.7f;
		leapingSlam.cooldownAfter = 0.3f;
		leapingSlam.damageDealt = 35.0f;
		leapingSlam.postureBreakDamage = 30.0f;  // High posture break
		leapingSlam.knockbackForce = 6.0f;
		leapingSlam.knocksDown = true;  // Can knock player down
		leapingSlam.canBeParried = false;  // Hard to parry an AOE
		leapingSlam.animationStateName = "Boss_LeapSlam";
		leapingSlam.particleEffectName = "GroundImpact";
		leapingSlam.soundEffectName = "boss_slam";
		leapingSlam.telegraphVFXName = "RedGlow_Large";
		leapingSlam.minimumPlayerDistance = 3.0f;
		leapingSlam.maximumPlayerDistance = 12.0f;
		leapingSlam.preferredHealthThreshold = 0.5f;
		leapingSlam.phaseRequired = 2;
		leapingSlam.selectionWeight = 1.3f;
		RegisterPattern(leapingSlam);

		// ============================================================
		// PHASE 3: Ultimate attacks, high risk/reward
		// ============================================================

		// Pattern 6: Inferno Vortex (channeled, Dragon Ball inspired)
		AttackPattern infernoVortex;
		infernoVortex.name = "Inferno Vortex";
		infernoVortex.patternID = 6;
		infernoVortex.telegraphDuration = 0.8f;  // Long telegraph = time to escape
		infernoVortex.attackDuration = 1.5f;     // Channel effect
		infernoVortex.cooldownAfter = 0.6f;
		infernoVortex.damageDealt = 40.0f;
		infernoVortex.postureBreakDamage = 15.0f;
		infernoVortex.knockbackForce = 7.0f;
		infernoVortex.canBeParried = false;      // Can't parry channeled attack
		infernoVortex.animationStateName = "Boss_InfernoCharge";
		infernoVortex.particleEffectName = "InfernoVortex";
		infernoVortex.soundEffectName = "boss_inferno";
		infernoVortex.telegraphVFXName = "RedFlare_Intense";
		infernoVortex.minimumPlayerDistance = 2.0f;
		infernoVortex.maximumPlayerDistance = 10.0f;
		infernoVortex.preferredHealthThreshold = 0.3f;  // Use when desperate
		infernoVortex.phaseRequired = 3;
		infernoVortex.selectionWeight = 1.8f;
		RegisterPattern(infernoVortex);

		// Pattern 7: Desperate Fury Barrage (Dragon Ball finisher style)
		AttackPattern furyBarrage;
		furyBarrage.name = "Fury Barrage";
		furyBarrage.patternID = 7;
		furyBarrage.telegraphDuration = 0.4f;
		furyBarrage.attackDuration = 1.0f;  // Multiple hits rapid-fire
		furyBarrage.cooldownAfter = 0.5f;
		furyBarrage.damageDealt = 50.0f;     // High single hit damage
		furyBarrage.postureBreakDamage = 8.0f;  // Low posture (multiple rapid hits)
		furyBarrage.knockbackForce = 4.0f;
		furyBarrage.canBeParried = false;    // Too fast to parry
		furyBarrage.animationStateName = "Boss_FuryBarrage";
		furyBarrage.particleEffectName = "BlastBarrage";
		furyBarrage.soundEffectName = "boss_barrage";
		furyBarrage.telegraphVFXName = "RedStorm_Extreme";
		furyBarrage.minimumPlayerDistance = 4.0f;
		furyBarrage.maximumPlayerDistance = 15.0f;
		furyBarrage.preferredHealthThreshold = 0.2f;  // Use when boss is desperate
		furyBarrage.phaseRequired = 3;
		furyBarrage.selectionWeight = 2.0f;
		RegisterPattern(furyBarrage);
	}
}
