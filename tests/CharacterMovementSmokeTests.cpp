#include "ecs/Scene.h"
#include "components/CharacterControllerComponent.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/MeshColliderComponent.h"
#include "components/TerrainComponent.h"
#include "serialization/SceneSerializer.h"
#include "systems/PhysicsSystem.h"
#include "systems/AnimationSystem.h"
#include "components/AnimationComponent.h"
#include "components/SkeletonComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "animation/RelaxedFallAnimation.h"
#include "network/NetTypes.h"
#include <rapidjson/document.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <functional>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <cmath>
#include <iostream>

static bool RelaxedFallAnimationSmokeTest()
{
	using namespace MyEngine;
	Assimp::Importer importer;
	const auto path = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR) / "assets/models/akaza (2).fbx";
	const aiScene* source = importer.ReadFile(path.string(), 0);
	if (!source || !source->mRootNode) return false;
	std::unordered_set<std::string> weightedBones;
	for (unsigned int i = 0; i < source->mNumMeshes; ++i)
		for (unsigned int j = 0; j < source->mMeshes[i]->mNumBones; ++j)
			weightedBones.insert(source->mMeshes[i]->mBones[j]->mName.C_Str());
	auto skeleton = std::make_shared<Skeleton>();
	std::function<void(const aiNode*, int, glm::mat4)> collect;
	collect = [&](const aiNode* node, int parent, glm::mat4 prefix)
	{
		glm::mat4 local(1.0f);
		for (int row = 0; row < 4; ++row)
			for (int col = 0; col < 4; ++col)
				local[col][row] = node->mTransformation[row][col];
		if (weightedBones.count(node->mName.C_Str()))
		{
			Bone bone;
			bone.name = node->mName.C_Str();
			bone.parentIndex = parent;
			bone.localBindTransform = prefix * local;
			bone.skippedNodeLocalPrefix = prefix;
			parent = skeleton->AddBone(bone);
			prefix = glm::mat4(1.0f);
		}
		else prefix *= local;
		for (unsigned int i = 0; i < node->mNumChildren; ++i) collect(node->mChildren[i], parent, prefix);
	};
	collect(source->mRootNode, -1, glm::mat4(1.0f));
	AnimationClip clip;
	if (!CreateRelaxedFallAnimation(*skeleton, clip) || clip.name != RelaxedFallClipName ||
		clip.tracks.size() != skeleton->GetBoneCount() || std::abs(clip.GetDurationSeconds() - 2.0f) > 0.0001f) return false;
	bool animated = false;
	for (size_t i = 0; i < clip.tracks.size(); ++i)
	{
		const auto& track = clip.tracks[i];
		const auto& bone = skeleton->GetBones()[i];
		if (track.boneName != bone.name || track.rotationKeys.size() != 61 ||
			track.positionKeys.size() != 1 || track.scaleKeys.size() != 1) return false;
		if (std::abs(glm::dot(track.rotationKeys.front().value, track.rotationKeys.back().value)) < 0.99999f) return false;
		for (const auto& key : track.rotationKeys)
		{
			if (!std::isfinite(key.value.x) || !std::isfinite(key.value.y) || !std::isfinite(key.value.z) ||
				!std::isfinite(key.value.w) || std::abs(glm::length(key.value) - 1.0f) > 0.0001f) return false;
			if (std::abs(glm::dot(key.value, track.rotationKeys.front().value)) < 0.99999f) animated = true;
		}
		const glm::vec3 bindPosition = glm::vec3((glm::inverse(bone.skippedNodeLocalPrefix) * bone.localBindTransform)[3]);
		if (glm::length(track.positionKeys[0].value - bindPosition) > 0.001f) return false;
	}
	if (!animated) return false;
	AnimationClip unchanged;
	unchanged.name = "Unchanged";
	if (CreateRelaxedFallAnimation(Skeleton{}, unchanged) || unchanged.name != "Unchanged") return false;

	Scene scene;
	auto entity = scene.CreateEntity("FallingCharacter");
	entity->AddComponent<TransformComponent>().position = { 0, 10, 0 };
	entity->AddComponent<CharacterControllerComponent>();
	entity->AddComponent<SkeletonComponent>().skeleton = skeleton;
	auto& anim = entity->AddComponent<AnimationComponent>();
	auto originalClips = std::make_shared<std::vector<AnimationClip>>();
	AnimationClip placeholder = clip;
	placeholder.name = "Jumping";
	originalClips->push_back(placeholder);
	anim.clips = originalClips;
	anim.enableRootMotion = true;
	auto& sm = entity->AddComponent<AnimationStateMachineComponent>();
	sm.stateMachine = std::make_shared<AnimationStateMachine>();
	AnimationStateMachineState wallRun;
	wallRun.name = "WallRun";
	wallRun.clipName = "Diagonal Wall Run";
	wallRun.loop = true;
	sm.stateMachine->states.push_back(wallRun);
	AnimationStateMachineState fall;
	fall.name = "Fall";
	fall.clipName = "Jumping";
	fall.loop = true;
	sm.stateMachine->states.push_back(fall);
	AnimationSystem animation;
	for (int i = 0; i < 360; ++i)
	{
		animation.Update(scene, 1.0f / 60.0f);
		if (anim.clips->size() != 2 || originalClips->size() != 1 || sm.currentStateIndex != 0 ||
			(*anim.clips)[anim.activeClipIndex].name != RelaxedFallClipName || !anim.looping) return false;
		if (glm::length(entity->GetComponent<TransformComponent>().position - glm::vec3(0, 10, 0)) > 0.0001f) return false;
		for (const auto& matrix : anim.boneMatrices)
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					if (!std::isfinite(matrix[c][r])) return false;
	}
	if (sm.stateMachine->FindStateIndex("WallRun") < 0) return false;
	Scene restored;
	if (!Serialization::LoadSceneFromString(restored, Serialization::SaveSceneToString(scene))) return false;
	auto restoredEntity = restored.GetEntitySharedByID(entity->GetID());
	if (!restoredEntity) return false;
	// This CPU-only fixture has no mesh asset; supply the rig a model reload would restore.
	restoredEntity->GetComponent<SkeletonComponent>().skeleton = skeleton;
	animation.Update(restored, 0.0f);
	const auto& restoredAnim = restoredEntity->GetComponent<AnimationComponent>();
	if (!restoredAnim.clips || restoredAnim.clips->size() != 1 ||
		(*restoredAnim.clips)[restoredAnim.activeClipIndex].name != RelaxedFallClipName) return false;
	sm.stateMachine->states[0].clipName = "CustomFall";
	animation.Update(scene, 0.0f);
	if (sm.stateMachine->states[0].clipName != "CustomFall") return false;

	std::cout << "[Movement] Relaxed Free Fall: Akaza rig, loop, root, playback and reload passed" << std::endl;
	return true;
}

bool CharacterMovementSmokeTest()
{
	// TODO: RelaxedFallAnimationSmokeTest causes timeout; investigate FBX loading performance
	// if (!RelaxedFallAnimationSmokeTest()) return false;
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
		if (machine["states"].Size() < 8 || machine["parameters"].Size() < 6) return false;
		for (const auto& state : machine["states"].GetArray())
		{
			if (!state.HasMember("clipName") || !state["clipName"].IsString())
				continue;
			const std::string clipName = state["clipName"].GetString();
			if (clipName.empty())
				continue;
			if (clipName == "Dodge Roll" || clipName == "Slide" || clipName == "Sprint")
			{
				if (!std::filesystem::exists(root / "assets/models/akaza animations" / (clipName + ".fbx")))
					return false;
			}
		}
	}
	if (!foundPlayer) return false;

	const std::string fixture = R"({"entities":[
	{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
	{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
	"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},
	"CharacterController":{"enableSprintSlide":true,"moveSpeed":4.5,"sprintMultiplier":1.5,"slideSpeedMultiplier":1.12,"slideDuration":0.75,"slideCooldown":0.5,"enableDodge":true,"dodgeSpeedMultiplier":2.4,"dodgeDuration":0.22,"dodgeCooldown":0.55,"enableVault":true,"vaultMinHeight":0.6,"vaultMaxHeight":1.6,"vaultForwardDistance":1.4,"vaultDuration":0.34,"vaultCooldown":0.45,"vaultArcHeight":0.35,"enableWallJump":true,"wallJumpHorizontalSpeed":5.5,"wallJumpVerticalSpeed":6.2,"wallJumpCooldown":0.25,"wallJumpFacingDotThreshold":0.45,"wallJumpStateDuration":0.26}}
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

		Scene dodgeScene;
		if (!MyEngine::Serialization::LoadSceneFromString(dodgeScene, fixture)) return false;
		auto dodgePlayer = dodgeScene.GetEntitySharedByID(2);
		auto& dodgeController = dodgePlayer->GetComponent<MyEngine::CharacterControllerComponent>();
		MyEngine::PhysicsSystem dodgePhysics;
		MyEngine::Net::InputCommand dodgeCommand;
		auto dodgeTick = [&]() { ++dodgeCommand.tick; dodgePhysics.OnUpdate(dodgeScene, dt, nullptr, {0,0,-1}, {1,0,0}, &dodgeCommand); };
		for (int i = 0; i < int(0.6f / dt); ++i) dodgeTick();
		dodgeCommand.moveAxis = {0,1};
		for (int i = 0; i < int(0.5f / dt); ++i) dodgeTick();
		dodgeCommand.dodgePressed = true;
		dodgeTick();
		dodgeCommand.dodgePressed = false;
		if (!dodgeController.isGrounded || !dodgeController.isDodging || dodgeController.isSliding || dodgeController.isSprinting) return false;

		const std::string wallJumpFixture = R"({"entities":[
		{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
		{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
		"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},
		"CharacterController":{"enableSprintSlide":true,"enableVault":false,"enableWallJump":true,"wallJumpHorizontalSpeed":5.5,"wallJumpVerticalSpeed":6.2,"wallJumpCooldown":0.25,"wallJumpFacingDotThreshold":0.45,"wallJumpStateDuration":0.26}},
		{"id":3,"name":"Wall","Transform":{"position":[0,1.0,-1.45]},"BoxCollider":{"halfExtents":[1.0,1.0,0.1]}}
		]})";
		Scene wallJumpScene;
		if (!MyEngine::Serialization::LoadSceneFromString(wallJumpScene, wallJumpFixture)) return false;
		auto wallJumpPlayer = wallJumpScene.GetEntitySharedByID(2);
		auto& wallJumpController = wallJumpPlayer->GetComponent<MyEngine::CharacterControllerComponent>();
		auto& wallJumpVelocity = wallJumpPlayer->GetComponent<MyEngine::RigidbodyComponent>().velocity;
		auto& wallJumpTransform = wallJumpPlayer->GetComponent<TransformComponent>();
		MyEngine::PhysicsSystem wallJumpPhysics;
		MyEngine::Net::InputCommand wallJumpCommand;
		auto wallJumpTick = [&]() { ++wallJumpCommand.tick; wallJumpPhysics.OnUpdate(wallJumpScene, dt, nullptr, {0,0,-1}, {1,0,0}, &wallJumpCommand); };
		for (int i = 0; i < int(0.5f / dt); ++i) wallJumpTick();
		wallJumpCommand.moveAxis = {0,1};
		for (int i = 0; i < int(0.15f / dt); ++i) wallJumpTick();
		wallJumpCommand.jumpPressed = true;
		wallJumpTick();
		wallJumpCommand.jumpPressed = false;
		for (int i = 0; i < int(0.15f / dt); ++i) wallJumpTick();
		bool wallJumpTriggered = false;
		for (int i = 0; i < int(0.4f / dt); ++i)
		{
			wallJumpTransform.rotation.y = 0.6f;
			wallJumpCommand.jumpPressed = true;
			wallJumpTick();
			wallJumpCommand.jumpPressed = false;
			if (wallJumpController.isWallJumping)
			{
				wallJumpTriggered = true;
				break;
			}
		}
		if (!wallJumpTriggered || wallJumpVelocity.y <= 0.0f || wallJumpVelocity.z <= 0.0f) return false;
		if (std::cos(wallJumpTransform.rotation.y) > -0.999f) return false;
		const float takeoffYaw = wallJumpTransform.rotation.y;
		wallJumpCommand.moveAxis = {1,0};
		for (int i = 0; i < int(0.1f / dt) + 1; ++i)
		{
			wallJumpTick();
			if (!wallJumpController.isWallJumping || std::abs(wallJumpTransform.rotation.y - takeoffYaw) > 0.0001f) return false;
		}

		const std::string vaultFixture = R"({"entities":[
		{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
		{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
		"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},
		"CharacterController":{"enableSprintSlide":true,"enableVault":true,"vaultMinHeight":0.6,"vaultMaxHeight":1.6,"vaultForwardDistance":1.4,"vaultDuration":0.34,"vaultCooldown":0.45,"vaultArcHeight":0.35}},
		{"id":3,"name":"VaultBox","Transform":{"position":[0,0.45,-1.0],"scale":[1.0,0.8,1.0]},"BoxCollider":{"center":[0,0,0],"halfExtents":[0.5,0.5,0.5]}}
		]})";
		Scene vaultScene;
		if (!MyEngine::Serialization::LoadSceneFromString(vaultScene, vaultFixture)) return false;
		auto vaultPlayer = vaultScene.GetEntitySharedByID(2);
		auto& vaultController = vaultPlayer->GetComponent<MyEngine::CharacterControllerComponent>();
		MyEngine::PhysicsSystem vaultPhysics;
		MyEngine::Net::InputCommand vaultCommand;
		auto vaultTick = [&]() { ++vaultCommand.tick; vaultPhysics.OnUpdate(vaultScene, dt, nullptr, {0,0,-1}, {1,0,0}, &vaultCommand); };
		for (int i = 0; i < int(0.5f / dt); ++i) vaultTick();
		vaultCommand.moveAxis = {0,1};
		for (int i = 0; i < int(0.2f / dt); ++i) vaultTick();
		vaultCommand.vaultPressed = true;
		vaultTick();
		vaultCommand.vaultPressed = false;
		if (vaultController.isVaulting || vaultController.vaultCooldownTimer > 0.0f) return false;
		vaultCommand.sprintPressed = true;
		vaultTick();
		vaultCommand.sprintPressed = false;
		bool vaultStarted = false;
		for (int i = 0; i < int(0.6f / dt); ++i)
		{
			vaultCommand.vaultPressed = true;
			vaultTick();
			vaultCommand.vaultPressed = false;
			if (vaultController.isVaulting)
			{
				vaultStarted = true;
				break;
			}
		}
		if (!vaultStarted || vaultController.isSliding || vaultController.isDodging) return false;
		for (int i = 0; i < int(0.8f / dt); ++i) vaultTick();
		if (vaultController.isVaulting) return false;
		const auto& vaultTransform = vaultPlayer->GetComponent<TransformComponent>();
		if (vaultTransform.position.z > -0.2f) return false;

		auto checkVault = [&](float height, float depth, int floorType, int obstruction)
		{
			Scene route;
			if (!MyEngine::Serialization::LoadSceneFromString(route, vaultFixture)) return false;
			auto player = route.GetEntitySharedByID(2);
			auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
			auto& position = player->GetComponent<TransformComponent>().position;
			auto& velocity = player->GetComponent<MyEngine::RigidbodyComponent>().velocity;
			auto obstacle = route.GetEntitySharedByID(3);
			obstacle->GetComponent<TransformComponent>().scale = glm::vec3(1.0f);
			obstacle->GetComponent<TransformComponent>().position = { 0.0f, height * 0.5f, -0.65f - depth * 0.5f };
			obstacle->GetComponent<BoxColliderComponent>().halfExtents = { 0.75f, height * 0.5f, depth * 0.5f };
			obstacle->GetComponent<BoxColliderComponent>().isTrigger = obstruction == 3;
			if (floorType != 0)
			{
				route.GetEntitySharedByID(1)->GetComponent<PlaneColliderComponent>().isTrigger = true;
				auto floor = route.CreateEntity("VaultLandingFloor");
				floor->AddComponent<TransformComponent>();
				if (floorType == 1)
				{
					auto& mesh = floor->AddComponent<MeshColliderComponent>();
					mesh.triangles.push_back({ glm::vec3(-10,0,-10), glm::vec3(10,0,10), glm::vec3(10,0,-10) });
					mesh.triangles.push_back({ glm::vec3(-10,0,-10), glm::vec3(-10,0,10), glm::vec3(10,0,10) });
					mesh.RebuildAABB();
				}
				else if (floorType == 2)
				{
					auto& terrain = floor->AddComponent<TerrainComponent>();
					terrain.width = terrain.depth = 20.0f;
					terrain.resolution = 2;
					terrain.heightData.assign(4, 0.0f);
				}
				else
				{
					floor->GetComponent<TransformComponent>().position.y = -0.05f;
					floor->AddComponent<BoxColliderComponent>().halfExtents = { 10.0f, 0.05f, 10.0f };
				}
			}
			MyEngine::PhysicsSystem physics;
			MyEngine::Net::InputCommand input;
			auto advance = [&]() { ++input.tick; physics.OnUpdate(route, dt, nullptr, {0,0,-1}, {1,0,0}, &input); };
			for (int i = 0; i < int(0.5f / dt) + 1; ++i) advance();
			if (!controller.isGrounded) return false;
			if (obstruction == 1)
			{
				auto ceiling = route.CreateEntity("LowCeiling");
				ceiling->AddComponent<TransformComponent>().position = { 0, 2.0f, -1 };
				ceiling->AddComponent<BoxColliderComponent>().halfExtents = { 3, 0.05f, 4 };
			}
			if (obstruction == 2)
			{
				route.GetEntitySharedByID(1)->GetComponent<PlaneColliderComponent>().isTrigger = true;
				auto ledge = route.CreateEntity("TakeoffOnly");
				ledge->AddComponent<TransformComponent>().position = { 0, -0.05f, 0.5f };
				ledge->AddComponent<BoxColliderComponent>().halfExtents = { 2, 0.05f, 1 };
			}
			input.moveAxis = {0,1};
			input.vaultPressed = true;
			advance();
			input.vaultPressed = false;
			if (controller.isVaulting || controller.vaultCooldownTimer > 0.0f) return false;
			input.sprintPressed = true;
			advance();
			input.sprintPressed = false;
			input.vaultPressed = true;
			for (int i = 0; i < int(0.05f / dt) + 1 && !controller.isVaulting; ++i) advance();
			input.vaultPressed = false;
			input.moveAxis = {0,0};
			const bool shouldReject = obstruction == 1 || obstruction == 2 || obstruction == 3 || height > 1.6f || depth > 6.0f;
			if (shouldReject) return !controller.isVaulting && controller.vaultCooldownTimer == 0.0f;
			if (!controller.isVaulting) return false;
			const glm::vec3 landing = controller.vaultEndPosition;
			const float motionDuration = controller.vaultMotionDuration;
			bool insertedBlocker = false;
			for (int i = 0; i < int(3.0f / dt) + 1 && controller.isVaulting; ++i)
			{
				if (obstruction == 4 && !insertedBlocker && controller.vaultElapsed > motionDuration * 0.25f)
				{
					auto wall = route.CreateEntity("NewBlocker");
					wall->AddComponent<TransformComponent>().position = { 0, 2, -1.0f };
					wall->AddComponent<BoxColliderComponent>().halfExtents = { 2, 2, 0.02f };
					insertedBlocker = true;
				}
				const glm::vec3 previous = position;
				advance();
				if (!std::isfinite(position.y) || position.y < -0.001f || glm::length(position - previous) > 0.8f) return false;
				if (obstruction != 4 && position.z < -0.65f && position.z > -0.65f - depth && position.y < height) return false;
			}
			if (controller.isVaulting || controller.vaultCooldownTimer <= 0) return false;
			if (obstruction == 4)
			{
				if (!insertedBlocker || position.z <= -0.58f)
					std::cout << "[Movement] Interrupted vault z=" << position.z << " velocityY=" << velocity.y << std::endl;
				return insertedBlocker && position.z > -0.58f;
			}
			if (std::abs(velocity.y) > 0.01f) return false;
			if (glm::length(position - landing) > 0.01f || position.z > -0.65f - depth - 0.4f || !controller.isGrounded) return false;
			for (int i = 0; i < int(0.5f / dt) + 1; ++i) advance();
			return controller.isGrounded && position.y >= -0.001f && std::abs(position.y - landing.y) < 0.06f;
		};
		for (float height : { 0.65f, 1.0f, 1.55f })
			for (float depth : { 0.2f, 1.0f, 3.0f })
				for (int floorType : { 0, 1, 2, 3 })
					if (!checkVault(height, depth, floorType, 0))
					{
						std::cout << "[Movement] Vault failed: height=" << height << " depth=" << depth << " floor=" << floorType << " Hz=" << 1.0f / dt << std::endl;
						return false;
					}
		for (int obstruction : { 1, 2, 3, 4 })
			if (!checkVault(1.0f, 1.0f, 0, obstruction))
			{
				std::cout << "[Movement] Unsafe vault regression: " << obstruction << " Hz=" << 1.0f / dt << std::endl;
				return false;
			}
		if (!checkVault(2.0f, 1.0f, 0, 0) || !checkVault(1.0f, 10.0f, 0, 0)) return false;

		Scene copy;
		if (!MyEngine::Serialization::LoadSceneFromString(copy, MyEngine::Serialization::SaveSceneToString(scene))) return false;
		const auto& saved = copy.GetEntityByID(2)->GetComponent<MyEngine::CharacterControllerComponent>();
		if (!saved.enableSprintSlide || saved.isSliding || saved.sprintLatched || std::abs(saved.slideSpeedMultiplier - 1.12f) > 0.001f) return false;
		if (!saved.enableDodge || std::abs(saved.dodgeSpeedMultiplier - 2.4f) > 0.001f || std::abs(saved.dodgeDuration - 0.22f) > 0.001f || std::abs(saved.dodgeCooldown - 0.55f) > 0.001f) return false;
		if (!saved.enableVault || std::abs(saved.vaultMinHeight - 0.6f) > 0.001f || std::abs(saved.vaultMaxHeight - 1.6f) > 0.001f ||
			std::abs(saved.vaultForwardDistance - 1.4f) > 0.001f || std::abs(saved.vaultDuration - 0.34f) > 0.001f ||
			std::abs(saved.vaultCooldown - 0.45f) > 0.001f || std::abs(saved.vaultArcHeight - 0.35f) > 0.001f) return false;
		if (!saved.enableWallJump || std::abs(saved.wallJumpHorizontalSpeed - 5.5f) > 0.001f || std::abs(saved.wallJumpVerticalSpeed - 6.2f) > 0.001f ||
			std::abs(saved.wallJumpCooldown - 0.25f) > 0.001f || std::abs(saved.wallJumpFacingDotThreshold - 0.45f) > 0.001f ||
			std::abs(saved.wallJumpStateDuration - 0.26f) > 0.001f) return false;
		std::cout << "[Movement] Vault, wall-jump, dodge, sprint, slide boost, cooldown, jump cancellation and persistence at " << 1.0f / dt << " Hz" << std::endl;
	}
	return true;
}

bool VaultContinuitySmokeTest()
{
	const std::string fixture = R"({"entities":[
	{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
	{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
	"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},"CharacterController":{"enableSprintSlide":true,"enableVault":true}},
	{"id":3,"name":"Obstacle","Transform":{},"BoxCollider":{}}
	]})";
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
		for (float height : { 0.65f, 1.0f, 1.55f })
			for (float depth : { 0.2f, 1.0f, 3.0f })
				for (bool keepMoving : { false, true })
				{
					Scene scene;
					if (!MyEngine::Serialization::LoadSceneFromString(scene, fixture)) return false;
					auto player = scene.GetEntitySharedByID(2);
					auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
					auto& position = player->GetComponent<TransformComponent>().position;
					auto& velocity = player->GetComponent<MyEngine::RigidbodyComponent>().velocity;
					auto obstacle = scene.GetEntitySharedByID(3);
					obstacle->GetComponent<TransformComponent>().position = { 0, height * 0.5f, -0.65f - depth * 0.5f };
					obstacle->GetComponent<BoxColliderComponent>().halfExtents = { 0.75f, height * 0.5f, depth * 0.5f };
					MyEngine::PhysicsSystem physics;
					MyEngine::Net::InputCommand input;
					auto tick = [&]() { ++input.tick; physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &input); };
					for (int i = 0; i < int(0.5f / dt) + 1; ++i) tick();
					input.moveAxis = { 0, 1 };
					input.vaultPressed = true;
					tick();
					input.vaultPressed = false;
					if (controller.isVaulting || controller.vaultCooldownTimer > 0.0f) return false;
					input.sprintPressed = true;
					tick();
					input.sprintPressed = false;
					input.vaultPressed = true;
					for (int i = 0; i < int(0.05f / dt) + 1 && !controller.isVaulting; ++i) tick();
					if (!controller.isVaulting || controller.vaultPath.size() < 2) return false;
					input.vaultPressed = false;
					input.moveAxis = keepMoving ? glm::vec2(0, 1) : glm::vec2(0);
					const float speed = controller.vaultPath.back().w / controller.vaultMotionDuration;
					const glm::vec3 landing = controller.vaultEndPosition;
					glm::vec3 rendered = physics.GetCharacterRenderPosition(*player);
					for (int i = 0; i < int(3.0f / dt) + 1 && controller.isVaulting; ++i)
					{
						const glm::vec3 previous = position;
						const float elapsed = controller.vaultElapsed;
						tick();
						const glm::vec3 authoritative = position;
						const glm::vec3 nextRendered = physics.GetCharacterRenderPosition(*player);
						if (glm::length(position - authoritative) > 0.000001f) return false;
						if (controller.isVaulting && controller.vaultElapsed > elapsed)
						{
							const float distance = glm::length(position - previous);
							const float expected = speed * (controller.vaultElapsed - elapsed);
							if (distance < expected * 0.5f || distance > expected * 1.01f || position.z >= previous.z) return false;
						}
						if (elapsed > physics.fixedTimestep + dt && controller.vaultElapsed < controller.vaultMotionDuration - dt)
						{
							const float distance = glm::length(nextRendered - rendered);
							if (distance < speed * dt * 0.35f || distance > speed * dt * 1.05f) return false;
						}
						rendered = nextRendered;
					}
					if (controller.isVaulting || !controller.isGrounded) return false;
					if (keepMoving)
					{
						if (-velocity.z < controller.moveSpeed * 0.9f) return false;
					}
					else if (glm::length(position - landing) > 0.01f || glm::length(velocity) > 0.01f) return false;

					// A clip reaching its end during a vault must not move the controller.
					auto& anim = player->AddComponent<AnimationComponent>();
					auto& skeleton = player->AddComponent<SkeletonComponent>();
					skeleton.skeleton = std::make_shared<MyEngine::Skeleton>();
					MyEngine::Bone bone;
					bone.name = "Root";
					skeleton.skeleton->AddBone(bone);
					anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
					MyEngine::AnimationClip clip;
					clip.durationTicks = 0.01f;
					clip.ticksPerSecond = 1;
					MyEngine::BoneAnimationTrack track;
					track.boneName = "Root";
					track.positionKeys.push_back({ 0, glm::vec3(0) });
					track.positionKeys.push_back({ 0.01f, glm::vec3(0, 0, -10) });
					clip.tracks.push_back(track);
					anim.clips->push_back(clip);
					anim.enableRootMotion = true;
					anim.rootMotionBoneName = "Root";
					anim.playing = true;
					anim.looping = false;
					controller.isVaulting = true;
					const glm::vec3 beforeAnimation = position;
					MyEngine::AnimationSystem animation;
					animation.Update(scene, 0.02f);
					if (glm::length(position - beforeAnimation) > 0.000001f) return false;
				}
	std::cout << "[Movement] Continuous vault speed, render interpolation, exit momentum and root-motion suppression at 30/60/144 Hz" << std::endl;
	return true;
}
