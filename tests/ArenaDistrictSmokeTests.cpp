#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/CharacterControllerComponent.h"
#include "serialization/SceneSerializer.h"
#include "systems/PhysicsSystem.h"
#include "network/NetTransport.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

bool ArenaDistrictSmokeTest()
{
	const auto root = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR);
	std::ifstream input(root / "assets/models/akaza animations/akaza player2.json");
	const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	rapidjson::Document document;
	if (document.Parse(text.c_str()).HasParseError() || !document.HasMember("entities"))
		return false;

	int districtCount = 0;
	const std::set<std::string> physicsFields = { "id", "name", "tag", "layer", "Transform", "Rigidbody",
		"BoxCollider", "CapsuleCollider", "BoundingSphere", "PlaneCollider", "CharacterController" };
	for (auto& entity : document["entities"].GetArray())
	{
		if (entity.HasMember("tag") && std::string(entity["tag"].GetString()) == "ArenaDistrict")
		{
			++districtCount;
			if (entity.HasMember("BoxCollider"))
			{
				for (const auto& extent : entity["BoxCollider"]["halfExtents"].GetArray())
					if (std::abs(extent.GetFloat() - 0.5f) > 0.0001f)
						return false;
				if (!entity.HasMember("Rigidbody") || !entity["Rigidbody"]["isKinematic"].GetBool())
					return false;
			}
			else if (!entity.HasMember("BoundingSphere") || !entity["BoundingSphere"]["isTrigger"].GetBool())
			{
				std::cerr << "ArenaDistrictSmokeTest: decorative mesh would load a solid automatic collider" << std::endl;
				return false;
			}
			if (entity.HasMember("MeshRenderer") && entity["MeshRenderer"].HasMember("texturePath") &&
				!std::filesystem::exists(root / entity["MeshRenderer"]["texturePath"].GetString()))
				return false;
		}
		// Use the actual saved physics, omitting GPU imports and animation/script execution.
		for (auto member = entity.MemberBegin(); member != entity.MemberEnd();)
		{
			if (physicsFields.count(member->name.GetString()) == 0)
				member = entity.EraseMember(member);
			else
				++member;
		}
	}
	if (districtCount != 53 || document["entities"].Size() != 113)
		return false;

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	Scene scene;
	if (!MyEngine::Serialization::LoadSceneFromString(scene, buffer.GetString()))
		return false;
	std::shared_ptr<Entity> player;
	for (const auto& entity : scene.GetEntities())
		if (entity && entity->GetName() == "Player")
			player = entity;
	if (!player || !player->HasComponent<MyEngine::CharacterControllerComponent>())
		return false;

	auto& transform = player->GetComponent<TransformComponent>();
	auto& body = player->GetComponent<MyEngine::RigidbodyComponent>();
	auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
	const auto savedController = controller;
	MyEngine::PhysicsSystem physics;
	const float dt = 1.0f / 60.0f;
	auto tick = [&](const glm::vec3& direction)
	{
		MyEngine::Net::InputCommand command{};
		command.moveAxis = glm::vec2(direction.x, -direction.z);
		physics.OnUpdate(scene, dt, nullptr, glm::vec3(0, 0, -1), glm::vec3(1, 0, 0), &command);
	};

	struct Walk
	{
		const char* name;
		glm::vec3 start;
		glm::vec3 end;
	};
	const Walk walks[] = {
		{ "temple entrance and stairs", { 0, 0, -48 }, { 0, 0.9f, -57 } },
		{ "covered west gallery", { -28, 0, -14 }, { -40, 0, -14 } },
		{ "covered east market", { 28, 0, -14 }, { 39, 0, -14 } },
		{ "west rooftop bridge", { -20, 4, -24 }, { -38, 4, -24 } },
		{ "south courtyard expansion", { 6, 0, 8 }, { 6, 0, 27 } }
	};
	for (const auto& walk : walks)
	{
		transform.position = walk.start + glm::vec3(0, savedController.skinWidth, 0);
		body.velocity = glm::vec3(0);
		controller = savedController;
		for (int frame = 0; frame < 30; ++frame)
			tick(glm::vec3(0));
		for (int frame = 0; frame < 720; ++frame)
		{
			glm::vec3 direction = walk.end - transform.position;
			direction.y = 0;
			if (glm::length(direction) < 0.2f)
				break;
			tick(glm::normalize(direction));
			if (transform.position.y < walk.start.y - 0.1f)
			{
				std::cerr << "ArenaDistrictSmokeTest: lost floor support on " << walk.name << std::endl;
				return false;
			}
		}
		for (int frame = 0; frame < 30; ++frame)
			tick(glm::vec3(0));
		glm::vec3 remaining = walk.end - transform.position;
		remaining.y = 0;
		if (glm::length(remaining) > 0.5f || !controller.isGrounded ||
			std::abs(transform.position.y - walk.end.y - controller.skinWidth) > 0.12f)
		{
			std::cerr << "ArenaDistrictSmokeTest: blocked " << walk.name << " at " << transform.position.x << ','
				<< transform.position.y << ',' << transform.position.z << std::endl;
			return false;
		}
		std::cout << "[District] Walked " << walk.name << std::endl;
	}
	return true;
}
