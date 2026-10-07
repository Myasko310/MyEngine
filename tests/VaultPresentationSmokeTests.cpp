#include "ecs/Scene.h"
#include "components/CameraComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/MeshColliderComponent.h"
#include "serialization/SceneSerializer.h"
#include "systems/PhysicsSystem.h"
#include "systems/CameraSystem.h"
#include "core/PerformanceMetrics.h"
#include "network/NetTypes.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	const char* kVaultScene = R"({"entities":[
	{"id":1,"name":"Floor","Transform":{},"PlaneCollider":{"normal":[0,1,0],"distance":0}},
	{"id":2,"name":"Player","Transform":{"position":[0,0.03,0]},"Rigidbody":{"isKinematic":true,"useGravity":false},
	"CapsuleCollider":{"pointA":[0,0.4,0],"pointB":[0,1.4,0],"radius":0.4},"CharacterController":{"enableVault":true,"enableSprintSlide":true}},
	{"id":3,"name":"Obstacle","Transform":{"position":[0,0.5,-10]},"BoxCollider":{"halfExtents":[0.75,0.5,0.5]}}
	]})";
}

bool VaultApproachSmokeTest()
{
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
		for (bool sprint : { false, true })
		{
			Scene scene;
			if (!MyEngine::Serialization::LoadSceneFromString(scene, kVaultScene)) return false;
			auto player = scene.GetEntitySharedByID(2);
			auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
			auto& position = player->GetComponent<TransformComponent>().position;
			auto& velocity = player->GetComponent<MyEngine::RigidbodyComponent>().velocity;
			MyEngine::PhysicsSystem physics;
			MyEngine::Net::InputCommand input;
			auto tick = [&]() { ++input.tick; physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &input); };
			for (int i = 0; i < int(0.5f / dt) + 1; ++i) tick();
			input.moveAxis = {0,1};
			input.sprintPressed = sprint;
			tick();
			input.sprintPressed = false;
			for (int i = 0; i < int(0.5f / dt) + 1; ++i) tick();
			const float approachSpeed = -velocity.z;
			scene.GetEntitySharedByID(3)->GetComponent<TransformComponent>().position.z = position.z - 1.8f;
			input.vaultPressed = true;
			for (int i = 0; i < int(0.05f / dt) + 1 && !controller.isVaulting; ++i) tick();
			input.vaultPressed = false;
			if (!sprint)
			{
				if (controller.isVaulting || controller.vaultCooldownTimer > 0.0f) return false;
				continue;
			}
			if (!controller.isVaulting || controller.vaultPath.size() < 2) return false;
			const auto& path = controller.vaultPath;
			const glm::vec3 first = glm::normalize(glm::vec3(path[1] - path[0]));
			const glm::vec3 last = glm::normalize(glm::vec3(path.back() - path[path.size() - 2]));
			if (std::abs(first.y) > 0.4f || std::abs(last.y) > 0.4f || first.z >= 0 || last.z >= 0) return false;
			if (path.back().w / controller.vaultMotionDuration < approachSpeed * 1.15f) return false;
			for (int i = 0; i < int(3.0f / dt) + 1 && controller.isVaulting; ++i) tick();
			if (controller.isVaulting || !controller.isGrounded || -velocity.z < approachSpeed * 0.95f || controller.sprintLatched != sprint) return false;
		}

	// A scaled obstacle at close range must retain the original short-vault behavior.
	Scene scene;
	if (!MyEngine::Serialization::LoadSceneFromString(scene, kVaultScene)) return false;
	auto player = scene.GetEntitySharedByID(2);
	auto obstacle = scene.GetEntitySharedByID(3);
	obstacle->GetComponent<TransformComponent>().position = {0, 0.45f, -1};
	obstacle->GetComponent<TransformComponent>().scale = {1, 0.8f, 1};
	obstacle->GetComponent<BoxColliderComponent>().halfExtents = {0.5f, 0.5f, 0.5f};
	auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
	MyEngine::PhysicsSystem physics;
	MyEngine::Net::InputCommand input;
	auto tick = [&]() { ++input.tick; physics.OnUpdate(scene, 1.0f / 30, nullptr, {0,0,-1}, {1,0,0}, &input); };
	for (int i = 0; i < 15; ++i) tick();
	input.moveAxis = {0,1};
	for (int i = 0; i < 6; ++i) tick();
	input.vaultPressed = true;
	tick();
	input.vaultPressed = false;
	if (controller.isVaulting || controller.vaultCooldownTimer > 0.0f) return false;
	input.sprintPressed = true;
	tick();
	input.sprintPressed = false;
	for (int i = 0; i < 2; ++i) tick();
	input.vaultPressed = true;
	tick();
	input.vaultPressed = false;
	std::cout << "[Vault] Close obstacle: active=" << controller.isVaulting << " duration=" << controller.vaultMotionDuration << " path=" << (controller.vaultPath.empty() ? 0 : controller.vaultPath.back().w) << std::endl;
	if (!controller.isVaulting) return false;
	for (int i = 0; i < 24; ++i) tick();
	return !controller.isVaulting && controller.isGrounded;
}

bool VaultMovingMeshSmokeTest()
{
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
	{
		Scene scene;
		if (!MyEngine::Serialization::LoadSceneFromString(scene, kVaultScene)) return false;
		auto player = scene.GetEntitySharedByID(2);
		auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
		auto& position = player->GetComponent<TransformComponent>().position;
		scene.GetEntitySharedByID(3)->GetComponent<TransformComponent>().position.z = -1.15f;
		auto blocker = scene.CreateEntity("MovingMesh");
		blocker->AddComponent<TransformComponent>().position = {100,0,0};
		auto& mesh = blocker->AddComponent<MeshColliderComponent>();
		mesh.triangles.push_back({ glm::vec3(-2,0,0), glm::vec3(2,4,0), glm::vec3(2,0,0) });
		mesh.triangles.push_back({ glm::vec3(-2,0,0), glm::vec3(-2,4,0), glm::vec3(2,4,0) });
		mesh.RebuildAABB();
		MyEngine::PhysicsSystem physics;
		MyEngine::Net::InputCommand input;
		auto tick = [&]() { ++input.tick; physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &input); };
		for (int i = 0; i < int(0.5f / dt) + 1; ++i) tick();
		input.moveAxis = {0,1};
		input.sprintPressed = true;
		tick();
		input.sprintPressed = false;
		input.vaultPressed = true;
		for (int i = 0; i < int(0.05f / dt) + 1 && !controller.isVaulting; ++i) tick();
		input.vaultPressed = false;
		input.moveAxis = {0,0};
		if (!controller.isVaulting) return false;
		const float wallZ = position.z - 0.45f;
		blocker->GetComponent<TransformComponent>().position = {0,0,wallZ};
		for (int i = 0; i < int(3.0f / dt) + 1 && controller.isVaulting; ++i) tick();
		if (controller.isVaulting || controller.vaultCooldownTimer <= 0 || position.z < wallZ + 0.39f) return false;
	}
	return true;
}

bool VaultCameraSmokeTest()
{
	for (int hz : {30, 60, 144})
	{
		const float dt = 1.0f / hz;
		Scene scene;
		auto target = scene.CreateEntity("FollowTarget");
		auto& position = target->AddComponent<TransformComponent>().position;
		auto& controller = target->AddComponent<MyEngine::CharacterControllerComponent>();
		auto cameraEntity = scene.CreateEntity("Camera");
		auto& transform = cameraEntity->AddComponent<TransformComponent>();
		auto& camera = cameraEntity->AddComponent<MyEngine::CameraComponent>();
		camera.isPrimary = camera.thirdPerson = true;
		camera.enableInput = false;
		camera.followTargetID = target->GetID();
		camera.yaw = camera.pitch = 0;
		MyEngine::CameraSystem system;
		auto tick = [&]() { system.Update(scene, nullptr, dt, 16.0f / 9); };
		tick();
		controller.isVaulting = true;
		position = {0.1f, 0.2f, 0};
		tick();
		if (transform.position.y <= camera.followHeight || transform.position.y >= camera.followHeight + position.y) return false;
		if (std::abs(transform.position.x - (position.x - camera.followDistance)) > 0.0001f) return false;
		const float firstY = transform.position.y;
		controller.isVaulting = false;
		tick();
		if (transform.position.y < firstY || transform.position.y >= camera.followHeight + position.y) return false;
		for (int i = 0; i < hz; ++i) tick();
		if (std::abs(transform.position.y - (camera.followHeight + position.y)) > 0.001f) return false;
		position.y += 0.5f;
		tick();
		if (std::abs(transform.position.y - (camera.followHeight + position.y)) > 0.0001f) return false;
		controller.isVaulting = true;
		position.y += 0.2f;
		camera.yaw = 90;
		tick();
		if (std::abs(transform.position.z - (position.z - camera.followDistance)) > 0.0001f) return false;
		position.y += 100;
		tick();
		if (std::abs(transform.position.y - (camera.followHeight + position.y)) > 0.0001f) return false;
		auto replacement = scene.CreateEntity("Replacement");
		replacement->AddComponent<TransformComponent>().position = {0, -5, 0};
		replacement->AddComponent<MyEngine::CharacterControllerComponent>().isVaulting = true;
		camera.followTargetID = replacement->GetID();
		tick();
		if (std::abs(transform.position.y - (camera.followHeight - 5)) > 0.0001f) return false;
		camera.thirdPerson = false;
		tick();
		camera.thirdPerson = true;
		camera.followTargetID = target->GetID();
		tick();
		if (std::abs(transform.position.y - (camera.followHeight + position.y)) > 0.0001f) return false;
	}
	return true;
}

bool VaultTimingStatisticsSmokeTest()
{
	using MyEngine::Performance::PerformanceMetrics;
	const char* name = "Vault timing statistics test";
	for (int i = 1; i <= 100; ++i) PerformanceMetrics::RecordSample(name, static_cast<float>(i));
	auto stat = PerformanceMetrics::GetStat(name);
	if (stat.sampleCount != 100 || stat.minMs != 1 || stat.maxMs != 100 || stat.p95Ms != 95 || stat.p99Ms != 99 || stat.avgMs != 50.5f) return false;
	PerformanceMetrics::RecordSample(name, -1);
	PerformanceMetrics::RecordSample(name, std::numeric_limits<float>::infinity());
	for (int i = 101; i <= 400; ++i) PerformanceMetrics::RecordSample(name, static_cast<float>(i));
	stat = PerformanceMetrics::GetStat(name);
	return stat.sampleCount == 300 && stat.minMs == 101 && stat.maxMs == 400 && stat.p95Ms == 385 && stat.p99Ms == 397;
}

bool VaultArchSmokeTest()
{
	for (float dt : { 1.0f / 30, 1.0f / 60, 1.0f / 144 })
		for (float height : { 0.65f, 1.0f, 1.55f })
			for (float depth : { 0.2f, 1.0f, 3.0f })
			{
				Scene scene;
				if (!MyEngine::Serialization::LoadSceneFromString(scene, kVaultScene)) return false;
				auto player = scene.GetEntitySharedByID(2);
				auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
				auto& position = player->GetComponent<TransformComponent>().position;
				auto obstacle = scene.GetEntitySharedByID(3);
				obstacle->GetComponent<TransformComponent>().position = {0, height * 0.5f, -0.65f - depth * 0.5f};
				obstacle->GetComponent<BoxColliderComponent>().halfExtents = {0.75f, height * 0.5f, depth * 0.5f};
				MyEngine::PhysicsSystem physics;
				MyEngine::Net::InputCommand input;
				auto tick = [&]() { ++input.tick; physics.OnUpdate(scene, dt, nullptr, {0,0,-1}, {1,0,0}, &input); };
				for (int i = 0; i < int(0.5f / dt) + 1; ++i) tick();
				input.moveAxis = {0,1};
				input.sprintPressed = true;
				tick();
				input.sprintPressed = false;
				input.vaultPressed = true;
				for (int i = 0; i < int(0.05f / dt) + 1 && !controller.isVaulting; ++i) tick();
				input.vaultPressed = false;
				input.moveAxis = {0,0};
				if (!controller.isVaulting) return false;
				bool descending = false, risingOverWall = false, fallingOverWall = false;
				float maximumY = position.y;
				float previousSlope = std::numeric_limits<float>::max();
				for (size_t i = 1; i < controller.vaultPath.size(); ++i)
				{
					const auto& a = controller.vaultPath[i - 1];
					const auto& b = controller.vaultPath[i];
					const float dx = a.z - b.z;
					const float dy = b.y - a.y;
					if (dx <= 0 || b.w <= a.w) return false;
					if (descending && dy > 0.00001f) return false;
					if (dy < -0.00001f) descending = true;
					maximumY = std::max(maximumY, b.y);
					if (a.z < -0.65f && b.z > -0.65f - depth)
					{
						const float slope = dy / dx;
						if (slope >= previousSlope - 0.00001f || b.y < height) return false;
						previousSlope = slope;
						risingOverWall = risingOverWall || dy > 0.00001f;
						fallingOverWall = fallingOverWall || dy < -0.00001f;
					}
				}
				if (!descending || maximumY < height + std::max(controller.vaultArcHeight * 0.25f, 0.1f)) return false;
				if (depth >= 1.0f && (!risingOverWall || !fallingOverWall)) return false;
				const glm::vec3 landing = controller.vaultEndPosition;
				for (int i = 0; i < int(3.0f / dt) + 1 && controller.isVaulting; ++i) tick();
				if (controller.isVaulting || !controller.isGrounded || glm::length(position - landing) > 0.01f) return false;
			}
	return true;
}
