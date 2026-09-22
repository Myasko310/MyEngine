#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/CharacterControllerComponent.h"
#include "serialization/SceneSerializer.h"
#include "systems/PhysicsSystem.h"
#include "network/NetTransport.h"
#include "rendering/Shader.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
	std::shared_ptr<Entity> FindArenaEntity(Scene& scene, const std::string& name)
	{
		for (const auto& entity : scene.GetEntities())
			if (entity && entity->GetName() == name)
				return entity;
		return nullptr;
	}

	glm::vec3 ArenaTop(const Entity& entity)
	{
		const auto& transform = entity.GetComponent<TransformComponent>();
		return transform.position + glm::vec3(0.0f, transform.scale.y * 0.5f, 0.0f);
	}
}

bool StartupArenaSmokeTest()
{
	const auto scenePath = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR) / "assets/models/akaza animations/akaza player2.json";
	std::ifstream input(scenePath);
	const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	rapidjson::Document document;
	if (document.Parse(text.c_str()).HasParseError() || !document.HasMember("entities"))
		return false;

	std::set<unsigned> ids;
	int expansionCount = 0;
	for (auto& entity : document["entities"].GetArray())
	{
		if (!ids.insert(entity["id"].GetUint()).second)
			return false;
		if (entity.HasMember("tag") && std::string(entity["tag"].GetString()) == "ArenaExpansion")
		{
			++expansionCount;
			if (entity.HasMember("BoxCollider"))
			{
				for (const auto& extent : entity["BoxCollider"]["halfExtents"].GetArray())
					if (std::abs(extent.GetFloat() - 0.5f) > 0.0001f)
						return false;
				if (!entity.HasMember("Rigidbody") || !entity["Rigidbody"]["isKinematic"].GetBool())
					return false;
			}
		}
		// Load the saved physics components through the real serializer, without a GPU/model import.
		const std::set<std::string> physicsFields = { "id", "name", "tag", "layer", "Transform", "Rigidbody",
			"BoxCollider", "CapsuleCollider", "BoundingSphere", "PlaneCollider", "CharacterController" };
		for (auto member = entity.MemberBegin(); member != entity.MemberEnd();)
		{
			if (physicsFields.count(member->name.GetString()) == 0)
				member = entity.EraseMember(member);
			else
				++member;
		}
	}
	if (expansionCount != 48)
		return false;

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	const std::string physicsScene = buffer.GetString();
	for (const float frameDt : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f })
	{
		Scene scene;
		if (!MyEngine::Serialization::LoadSceneFromString(scene, physicsScene))
			return false;
		const auto player = FindArenaEntity(scene, "Player");
		if (!player || !player->HasComponent<MyEngine::CharacterControllerComponent>())
			return false;
		auto& transform = player->GetComponent<TransformComponent>();
		auto& body = player->GetComponent<MyEngine::RigidbodyComponent>();
		auto& controller = player->GetComponent<MyEngine::CharacterControllerComponent>();
		const auto savedController = controller;
		MyEngine::PhysicsSystem physics;
		auto tick = [&](const glm::vec3& direction, bool jump)
		{
			MyEngine::Net::InputCommand command{};
			command.moveAxis = glm::vec2(direction.x, -direction.z);
			command.jumpPressed = jump;
			physics.OnUpdate(scene, frameDt, nullptr, glm::vec3(0, 0, -1), glm::vec3(1, 0, 0), &command);
		};
		for (int frame = 0; frame < static_cast<int>(1.0f / frameDt); ++frame)
			tick(glm::vec3(0), false);
		const float spawnY = transform.position.y;
		float apexY = spawnY;
		bool airborne = false;
		for (int frame = 0; frame < static_cast<int>(2.0f / frameDt); ++frame)
		{
			tick(glm::vec3(0), frame == 0);
			apexY = std::max(apexY, transform.position.y);
			airborne = airborne || !controller.isGrounded;
		}
		if (!airborne || apexY - spawnY < 1.5f || !controller.isGrounded || std::abs(transform.position.y - spawnY) > 0.1f)
		{
			std::cerr << "StartupArenaSmokeTest: saved spawn jump/landing failed at dt=" << frameDt << std::endl;
			return false;
		}

		for (const std::string route : { "West", "East", "North" })
		{
			std::vector<std::shared_ptr<Entity>> steps;
			const std::string prefix = "Arena_" + route + "_0";
			for (const auto& entity : scene.GetEntities())
				if (entity && entity->GetName().find(prefix) == 0)
					steps.push_back(entity);
			std::sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) { return a->GetName() < b->GetName(); });
			if (steps.size() != 6)
				return false;
			transform.position = ArenaTop(*steps.front()) + (route == "North" ? glm::vec3(-2.7f, 0, 0) : glm::vec3(0, 0, 2.7f));
			transform.position.y = savedController.skinWidth;
			body.velocity = glm::vec3(0);
			controller = savedController;
			for (int frame = 0; frame < static_cast<int>(0.5f / frameDt); ++frame)
				tick(glm::vec3(0), false);

			for (const auto& step : steps)
			{
				const glm::vec3 target = ArenaTop(*step);
				const auto& scale = step->GetComponent<TransformComponent>().scale;
				bool landed = false;
				bool tookOff = false;
				for (int frame = 0; frame < static_cast<int>(2.0f / frameDt); ++frame)
				{
					glm::vec3 direction = target - transform.position;
					direction.y = 0;
					direction = glm::length(direction) > 0.2f ? glm::normalize(direction) : glm::vec3(0);
					tick(direction, frame == 0);
					tookOff = tookOff || !controller.isGrounded;
					if (tookOff && controller.isGrounded && std::abs(transform.position.y - target.y - controller.skinWidth) < 0.12f &&
						std::abs(transform.position.x - target.x) < scale.x * 0.5f && std::abs(transform.position.z - target.z) < scale.z * 0.5f)
					{
						landed = true;
						break;
					}
				}
				if (!landed)
				{
					std::cerr << "StartupArenaSmokeTest: cannot traverse " << step->GetName() << " at dt=" << frameDt
						<< " position=" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << std::endl;
					return false;
				}
				// Walk to the landing center before the next jump; no teleport or velocity reset between hops.
				for (int frame = 0; frame < static_cast<int>(1.5f / frameDt); ++frame)
				{
					glm::vec3 direction = target - transform.position;
					direction.y = 0;
					tick(glm::length(direction) > 0.2f ? glm::normalize(direction) : glm::vec3(0), false);
				}
				if (!controller.isGrounded || std::abs(transform.position.y - target.y - controller.skinWidth) > 0.12f)
					return false;
			}
			std::cout << "[Arena] Traversed " << route << " route at " << 1.0f / frameDt << " Hz input" << std::endl;
		}

		const auto drop = scene.CreateEntity("ArenaDynamicCollisionProbe");
		auto& dropTransform = drop->AddComponent<TransformComponent>();
		dropTransform.position = glm::vec3(9, 7, -37);
		drop->AddComponent<BoundingSphereComponent>().radius = 0.25f;
		auto& dropBody = drop->AddComponent<MyEngine::RigidbodyComponent>();
		dropBody.bounciness = 0;
		transform.position = glm::vec3(0, 0.03f, -20);
		body.velocity = glm::vec3(0);
		for (int frame = 0; frame < static_cast<int>(2.0f / frameDt); ++frame)
			tick(glm::vec3(0), false);
		if (std::abs(dropTransform.position.y - 4.25f) > 0.1f)
		{
			std::cerr << "StartupArenaSmokeTest: dynamic body fell through podium" << std::endl;
			return false;
		}
	}
	return true;
}

bool ShaderPollingSmokeTest()
{
	if (!glfwInit())
		return false;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(32, 32, "ShaderPollingSmokeTest", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	const auto directory = std::filesystem::temp_directory_path() / ("MyEngineShaderSmoke_" +
		std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto vertex = directory / "test.vert";
	const auto fragment = directory / "test.frag";
	std::ofstream(vertex) << "#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}";
	const std::string validFragment = "#version 330 core\nout vec4 color; void main(){color=vec4(1);}";
	std::ofstream(fragment) << validFragment;
	bool passed = false;
	{
		MyEngine::Shader shader(vertex.string(), fragment.string());
		const auto original = shader.GetID();
		const bool unchanged = !shader.TryHotReloadFromDisk();
		std::filesystem::last_write_time(fragment, std::filesystem::last_write_time(fragment) + std::chrono::seconds(2));
		const bool throttled = !shader.TryHotReloadFromDisk() && shader.GetID() == original;
		std::this_thread::sleep_for(std::chrono::milliseconds(275));
		const bool reloaded = shader.TryHotReloadFromDisk() && shader.GetID() != original;
		const auto goodProgram = shader.GetID();
		std::ofstream(fragment) << "invalid shader";
		const bool failedSafely = !shader.ReloadFromDisk() && shader.GetID() == goodProgram;
		std::ofstream(fragment) << validFragment;
		const bool manualReload = shader.ReloadFromDisk();
		passed = original != 0 && unchanged && throttled && reloaded && failedSafely && manualReload;
	}
	std::filesystem::remove_all(directory);
	glfwDestroyWindow(window);
	glfwTerminate();
	return passed;
}
