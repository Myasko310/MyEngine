#pragma once
#include "ai/EncounterMemory.h"
#include "components/EncounterSteering.h"

#include <cstdint>
#include <glm/glm.hpp>

class NavMeshSystem;

namespace MyEngine
{
	struct SparringPartnerComponent
	{
		EncounterMemory memory;
		const NavMeshSystem* navigation = nullptr;
		EncounterSteering traversal;
		std::vector<glm::vec3> route;
		std::size_t node = 0;
		float replan = 0, progressTime = 0, escapeTime = 0;
		glm::vec3 checkpoint{0}, escape{0};
		int escapeSide = 1;
		bool addedAttack = false, addedHitbox = false, addedHurtbox = false, addedStats = false;
		bool addedStrikeBones = false;
		bool infiniteHealth = true;
		static constexpr float openingDelay = .70f;
		static constexpr float repeatDelay = 1.10f;
		float initiativeDelay = openingDelay;
		bool closingForJab = false;
		bool rangedAttack = false;
		struct Beat { int slot; bool burst; };
		inline static constexpr Beat combos[4][3] = {
			{{0,false},{1,false},{1,true}}, {{0,false},{0,true},{1,false}},
			{{0,true},{0,false},{1,false}}, {{1,true},{0,true},{0,false}}
		};
		int combo = -1, nextBeat = 0;
		unsigned closeStrings = 0, rangedStrings = 0;
		float followupTime = 0;
		int Opening(bool ranged) const { return ranged ? 2 + int(rangedStrings%2) : int(closeStrings%2); }
		void CommitBeat(int pattern)
		{
			if (combo < 0) { ++(pattern >= 2 ? rangedStrings : closeStrings); combo = pattern; }
			if (++nextBeat == 3) { EndCombo(); initiativeDelay = repeatDelay; }
			else { followupTime = 2.f; initiativeDelay = 0; }
		}
		void EndCombo() { combo = -1; nextBeat = 0; followupTime = 0; }
		bool responding = false, counterSpent = false;
		bool dodgeIssued = false;
		struct Trial { bool pending = false, active = false, hit = false; std::uint32_t activation = 0; } dodgeTrial, counterTrial;
		int actualLane = 0, laneScore[2]{};
		unsigned laneChoices = 0;
		float ChooseLane()
		{
			++laneChoices;
			if (laneScore[0] == laneScore[1] || laneChoices % 4 == 0) return -evadeSide;
			return laneScore[0] > laneScore[1] ? -1.0f : 1.0f;
		}
		void FinishTrial(Trial& trial, bool dodge)
		{
			if (trial.active)
			{
				if (dodge) laneScore[actualLane] = std::clamp(laneScore[actualLane] + (trial.hit ? -2 : 1), -6, 6);
				RecordDecision(dodge ? (trial.hit ? "Dodge outcome: hit" : "Dodge outcome: swing missed") : (trial.hit ? "Counter outcome: hit" : "Counter outcome: missed"));
			}
			trial = {};
		}
		bool flanking = false;
		int flankSide = 1;
		bool samplingTarget = false;
		glm::vec3 targetSample{}, targetHeading{};
		float flankHoldoff = 0;
		bool DoubledBack(glm::vec3 target)
		{
			target.y = 0;
			if (!samplingTarget) { samplingTarget = true; targetSample = target; return false; }
			const auto delta = target - targetSample;
			const float length = glm::length(delta);
			if (length < 0.45f) return false;
			const auto heading = delta / length;
			const bool reversed = length < 3 && glm::dot(heading, targetHeading) < -0.65f;
			targetSample = target; targetHeading = length < 3 ? heading : glm::vec3(0);
			return reversed;
		}
		float encounterTime = 0;
		std::vector<std::string> decisions;
		void RecordDecision(const char* message)
		{
			if (decisions.size() == 48) decisions.erase(decisions.begin());
			const int tenths = static_cast<int>(encounterTime * 10);
			decisions.push_back(std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s  " + message);
		}
		struct LaneProbe { glm::vec3 end{}; bool clear = false; };
		LaneProbe laneProbes[2]{};
		glm::vec3 laneStart{};
		int laneCount = 0;
		float laneDisplayTime = 0;
		float counterWindow = 0.0f;
		int lastSwing = -1;
		unsigned repeatCount = 0;
		float patternTime = 0;
		const char* dodgeReason = "Waiting for a nearby swing";
		const char* counterReason = "No recovery opening yet";
		void WitnessSwing(int attack)
		{
			repeatCount = attack == lastSwing && patternTime > 0 ? (std::min)(repeatCount + 1, 8u) : 1;
			lastSwing = attack;
			patternTime = 4.0f;
		}
		glm::vec3 DodgeLane(float sign) const
		{
			const glm::vec3 lateral = glm::vec3(-facing.z, 0, facing.x) * evadeSide * sign;
			return repeatCount >= 2 ? glm::normalize(lateral + 0.45f * facing) : lateral;
		}
		float evadeSide = 1.0f;
		unsigned spacingDecisions = 0;
		float sideBias = 0.0f;
		enum class Footwork { Hold, Approach, Retreat };
		std::uint32_t playerId = 0;
		Footwork footwork = Footwork::Hold;
		glm::vec3 facing{0.0f, 0.0f, 1.0f};
	};
}
