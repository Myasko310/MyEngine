#include "ecs/Scene.h"
#include "components/CharacterControllerComponent.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "serialization/SceneSerializer.h"
#include "systems/PhysicsSystem.h"
#include "systems/AnimationSystem.h"
#include "components/AnimationComponent.h"
#include "components/SkeletonComponent.h"
#include "network/NetTypes.h"
#include <rapidjson/document.h>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <cmath>
#include <iostream>

bool CharacterMovementSmokeTest()
{
	// Reproduce Slide -> Run -> Walk -> Idle while the first fade is incomplete.
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
	{
		Scene scene;
		auto entity = scene.CreateEntity("InterruptedSlideRecovery");
		auto& anim = entity->AddComponent<AnimationComponent>();
		auto& skeleton = entity->AddComponent<SkeletonComponent>();
		skeleton.skeleton = std::make_shared<MyEngine::Skeleton>();
		MyEngine::Bone rootBone;
		rootBone.name = "Root";
		skeleton.skeleton->AddBone(rootBone);
		anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
		for (float height : { 0.0f, 10.0f, 6.0f, 8.0f })
		{
			MyEngine::AnimationClip clip;
			clip.durationTicks = 10;
			clip.ticksPerSecond = 1;
			MyEngine::BoneAnimationTrack track;
			track.boneName = "Root";
			track.positionKeys.push_back({ 0, glm::vec3(0, height, 0) });
			clip.tracks.push_back(track);
			anim.clips->push_back(clip);
		}
		MyEngine::AnimationSystem animation;
		animation.Update(scene, dt);
		anim.TransitionTo(1, 0.4f);
		for (int frame = 0; frame < 3; ++frame) animation.Update(scene, dt);
		for (int destination : { 2, 3 })
		{
			const float previousHeight = anim.boneMatrices[0][3].y;
			anim.TransitionTo(destination, 0.4f);
			animation.Update(scene, 0.0f);
			if (std::abs(anim.boneMatrices[0][3].y - previousHeight) > 0.0001f) return false;
			animation.Update(scene, dt);
			const float targetHeight = destination == 2 ? 6.0f : 8.0f;
			const float expected = previousHeight + (targetHeight - previousHeight) * dt / 0.4f;
			if (std::abs(anim.boneMatrices[0][3].y - expected) > 0.0001f) return false;
		}
		for (int frame = 0; frame < int(0.5f / dt) + 1; ++frame) animation.Update(scene, dt);
		if (anim.blending || !anim.blendSourcePose.empty() || std::abs(anim.boneMatrices[0][3].y - 8.0f) > 0.0001f) return false;
		// An uninterrupted slide-to-sprint fade must retain its authored duration.
		anim.TransitionTo(0, 0.4f);
		animation.Update(scene, 0.2f);
		if (std::abs(anim.boneMatrices[0][3].y - 4.0f) > 0.0001f) return false;
		animation.Update(scene, 0.2f);
		if (anim.blending || std::abs(anim.boneMatrices[0][3].y) > 0.0001f) return false;
	}
	const auto root = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR);
	std::ifstream input(root / "assets/models/akaza animations/akaza player2.json");
	const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	rapidjson::Document doc;
	if (doc.Parse(text.c_str()).HasParseError()) return false;
	bool foundPlayer = false;
	for (const auto& entity : doc["entities"].GetArray())
	{
		if (std::string(entity["name"].GetString()) != "Player") continue;
		foundPlayer = true;
		if (!entity["CharacterController"]["enableSprintSlide"].GetBool()) return false;
		const auto& machine = entity["AnimationStateMachineComponent"]["stateMachineData"];
		if (machine["states"].Size() != 18 || machine["parameters"].Size() != 10) return false;
		for (int i = 0; i < 6; ++i)
		{
			const auto& transitions = machine["states"][i]["transitions"];
			if (transitions[0]["targetStateIndex"].GetInt() != 17 || transitions[1]["targetStateIndex"].GetInt() != 16) return false;
		}
		for (int i = 16; i <= 17; ++i)
		{
			const auto& state = machine["states"][i];
			if (!std::filesystem::exists(root / "assets/models/akaza animations" / (std::string(state["clipName"].GetString()) + ".fbx"))) return false;
			for (const auto& transition : state["transitions"].GetArray())
				if (transition["targetStateIndex"].GetInt() == i || transition["requiresExitTime"].GetBool()) return false;
		}
		const auto& exits = machine["states"][17]["transitions"];
		const int expectedTargets[] = { 15, 16, 4, 3, 2, 1, 0 };
		if (exits.Size() != 7) return false;
		for (int i = 0; i < 7; ++i)
		{
			if (exits[i]["targetStateIndex"].GetInt() != expectedTargets[i]) return false;
			if (i == 0)
			{
				if (exits[i]["blendDuration"].GetFloat() > 0.2f) return false;
			}
			else if (std::abs(exits[i]["blendDuration"].GetFloat() - 0.4f) > 0.001f)
				return false;
		}
		auto recoveryTarget = [&](bool sprinting, bool crouching, float speed)
		{
			for (const auto& transition : exits.GetArray())
			{
				bool matches = true;
				for (const auto& condition : transition["conditions"].GetArray())
				{
					const std::string name = condition["parameterName"].GetString();
					const std::string op = condition["op"].GetString();
					if (name == "Jump") matches = false;
					else if (name == "Speed") matches &= op == "Greater" && speed > condition["threshold"].GetFloat();
					else
					{
						const bool value = name == "IsSprinting" ? sprinting : name == "IsCrouching" ? crouching : false;
						matches &= op == "IfTrue" ? value : !value;
					}
				}
				if (matches) return transition["targetStateIndex"].GetInt();
			}
			return -1;
		};
		if (recoveryTarget(true, false, 6.75f) != 16 || recoveryTarget(false, true, 4.5f) != 4 ||
			recoveryTarget(false, true, 0.0f) != 3 || recoveryTarget(false, false, 4.5f) != 2 ||
			recoveryTarget(false, false, 1.0f) != 1 || recoveryTarget(false, false, 0.0f) != 0)
			return false;
	}
	if (!foundPlayer) return false;

	const std::string fixture = R"({"entities":[
	{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
	{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
	"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},
	"CharacterController":{"enableSprintSlide":true,"moveSpeed":4.5,"sprintMultiplier":1.5,"slideSpeedMultiplier":1.12,"slideDuration":0.75,"slideCooldown":0.5}}
	]})";
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
	{
		Scene scene;
		if (!MyEngine::Serialization::LoadSceneFromString(scene, fixture)) return false;
		auto player = scene.GetEntitySharedByID(2);
		auto& c = player->GetComponent<MyEngine::CharacterControllerComponent>();
		auto& rb = player->GetComponent<MyEngine::RigidbodyComponent>();
		MyEngine::PhysicsSystem physics;
		MyEngine::Net::InputCommand command;
		auto tick = [&]() { ++command.tick; physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &command); };
		for (int i = 0; i < int(0.5f / dt); ++i) tick();
		command.slidePressed = true;
		tick();
		command.slidePressed = false;
		if (c.isSliding) return false;
		command.moveAxis = {0,1};
		for (int i = 0; i < int(1.0f / dt); ++i) tick();
		if (std::abs(c.currentSpeed - 4.5f) > 0.1f) return false;
		// Crouch at full running speed must not start a slide or queue one for later.
		command.slidePressed = true;
		command.crouchHeld = true;
		tick();
		command.slidePressed = false;
		for (int i = 0; i < int(0.1f / dt); ++i) tick();
		if (c.isSliding) return false;
		command.sprintPressed = true;
		tick();
		// A reused authoritative tick must not toggle sprint a second time.
		physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &command);
		command.sprintPressed = false;
		for (int i = 0; i < int(0.2f / dt); ++i) tick();
		if (c.isSliding || c.isSprinting) return false;
		command.crouchHeld = false;
		for (int i = 0; i < int(0.5f / dt); ++i) tick();
		if (!c.isSprinting || std::abs(c.currentSpeed - 6.75f) > 0.1f) return false;
		command.slidePressed = true;
		command.crouchHeld = true;
		tick();
		command.slidePressed = false;
		for (int i = 0; i < int(0.2f / dt); ++i) tick();
		if (!c.isSliding || c.isSprinting || std::abs(c.currentSpeed - 7.56f) > 0.12f) return false;
		for (int i = 0; i < int(0.65f / dt); ++i) tick();
		if (c.isSliding) return false;
		command.slidePressed = true;
		tick();
		command.slidePressed = false;
		if (c.isSliding) return false;
		command.crouchHeld = false;
		for (int i = 0; i < int(0.7f / dt); ++i) tick();
		command.slidePressed = true;
		command.crouchHeld = true;
		tick();
		command.slidePressed = false;
		for (int i = 0; i < int(0.1f / dt); ++i) tick();
		if (!c.isSliding) return false;
		command.jumpPressed = true;
		tick();
		command.jumpPressed = false;
		for (int i = 0; i < int(0.1f / dt); ++i) tick();
		if (c.isSliding || c.isGrounded || rb.velocity.y <= 0) return false;
		command.slidePressed = true;
		tick();
		command.slidePressed = false;
		if (c.isSliding) return false;
		command.moveAxis = {0,0};
		for (int i = 0; i < int(2.0f / dt); ++i) tick();
		if (c.sprintLatched || c.isSprinting || c.currentSpeed > 0.1f) return false;
		Scene copy;
		if (!MyEngine::Serialization::LoadSceneFromString(copy, MyEngine::Serialization::SaveSceneToString(scene))) return false;
		const auto& saved = copy.GetEntityByID(2)->GetComponent<MyEngine::CharacterControllerComponent>();
		if (!saved.enableSprintSlide || saved.isSliding || saved.sprintLatched || std::abs(saved.slideSpeedMultiplier - 1.12f) > 0.001f) return false;
		std::cout << "[Movement] Sprint, slide boost, cooldown, jump cancellation and persistence at " << 1.0f / dt << " Hz" << std::endl;
	}
	return true;
}
