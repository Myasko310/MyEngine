#include "renderer/RenderBounds.h"
#include "renderer/FrustumCuller.h"
#include "editor/SceneHierarchyIndex.h"
#include "ecs/TransformHierarchy.h"

#include <glm/gtc/matrix_transform.hpp>
#include <rapidjson/document.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

bool SceneVisibilitySmokeTest()
{
	const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 120.0f);
	MyEngine::FrustumCuller culler;
	culler.Update(projection);
	const auto large = MyEngine::TransformSphereBounds(
		glm::scale(glm::translate(glm::mat4(1), glm::vec3(-10, 0, -5)), glm::vec3(20, 1, 1)), glm::vec3(0), 0.8660254f);
	if (!culler.IsSphereVisible(large.center, large.radius) || culler.IsSphereVisible(glm::vec3(-10, 0, -5), 0.8660254f))
		return false;
	if (culler.IsSphereVisible(glm::vec3(1000, 0, -5), 0.5f))
		return false;

	Scene scene;
	auto parent = scene.CreateEntity("ScaledParent");
	auto& parentTransform = parent->AddComponent<TransformComponent>();
	parentTransform.position = glm::vec3(4, 2, -20);
	parentTransform.rotation = glm::vec3(0.2f, 0.6f, 0.1f);
	parentTransform.scale = glm::vec3(-8, 2, 0.5f);
	auto child = scene.CreateEntity("RotatedChild");
	auto& childTransform = child->AddComponent<TransformComponent>();
	childTransform.parentID = parent->GetID();
	childTransform.position = glm::vec3(2, 1, -1);
	childTransform.rotation = glm::vec3(0.3f, 0.1f, 0.8f);
	childTransform.scale = glm::vec3(0.5f, 3, 2);
	const glm::mat4 world = TransformHierarchy::GetWorldMatrix(scene, *child);
	const glm::vec3 localCenter(0.25f, 0.5f, -0.1f);
	const float localRadius = 0.8660254f;
	const auto transformed = MyEngine::TransformSphereBounds(world, localCenter, localRadius);
	if (glm::length(transformed.center - glm::vec3(world * glm::vec4(localCenter, 1))) > 0.0001f)
		return false;
	for (int latitude = 0; latitude <= 16; ++latitude)
	{
		for (int longitude = 0; longitude < 32; ++longitude)
		{
			const float theta = glm::radians(latitude * 180.0f / 16.0f);
			const float phi = glm::radians(longitude * 360.0f / 32.0f);
			const glm::vec3 direction(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
			const glm::vec3 point(world * glm::vec4(localCenter + direction * localRadius, 1));
			if (glm::length(point - transformed.center) > transformed.radius + 0.0001f)
				return false;
		}
	}
	const auto uniform = MyEngine::TransformSphereBounds(glm::scale(glm::mat4(1), glm::vec3(-3, 3, 3)), glm::vec3(0), 2);
	if (std::abs(uniform.radius - 6) > 0.0001f)
		return false;

	const auto path = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR) / "assets/models/akaza animations/akaza player2.json";
	std::ifstream input(path);
	const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	rapidjson::Document document;
	if (document.Parse(text.c_str()).HasParseError() || !document.HasMember("entities"))
		return false;
	int visibleCornerCases = 0;
	int reproducedLocalBoundsFailures = 0;
	for (int angle = 0; angle < 360; angle += 15)
	{
		const float radians = glm::radians(static_cast<float>(angle));
		const glm::vec3 outward(std::cos(radians), 0, std::sin(radians));
		const glm::vec3 camera = glm::vec3(0, 3, -18) + outward * 20.0f;
		const glm::mat4 vp = projection * glm::lookAt(camera, camera + outward, glm::vec3(0, 1, 0));
		culler.Update(vp);
		for (const auto& entity : document["entities"].GetArray())
		{
			if (!entity.HasMember("tag") || !entity.HasMember("BoxCollider"))
				continue;
			const std::string tag = entity["tag"].GetString();
			if (tag != "ArenaExpansion" && tag != "ArenaDistrict")
				continue;
			const auto& saved = entity["Transform"];
			auto vec3 = [](const rapidjson::Value& value) { return glm::vec3(value[0].GetFloat(), value[1].GetFloat(), value[2].GetFloat()); };
			TransformComponent transform;
			transform.position = vec3(saved["position"]);
			transform.scale = vec3(saved["scale"]);
			if (saved.HasMember("rotation"))
				transform.rotation = vec3(saved["rotation"]);
			if (saved.HasMember("parentID") && saved["parentID"].GetUint() != 0)
				return false;
			const glm::mat4 matrix = transform.GetMatrix();
			const auto bounds = MyEngine::TransformSphereBounds(matrix, glm::vec3(0), 0.8660254f);
			for (int corner = 0; corner < 8; ++corner)
			{
				const glm::vec3 local((corner & 1) ? 0.5f : -0.5f, (corner & 2) ? 0.5f : -0.5f, (corner & 4) ? 0.5f : -0.5f);
				const glm::vec4 clip = vp * matrix * glm::vec4(local, 1);
				if (clip.w <= 0 || std::abs(clip.x) >= clip.w || std::abs(clip.y) >= clip.w || std::abs(clip.z) >= clip.w)
					continue;
				++visibleCornerCases;
				if (!culler.IsSphereVisible(bounds.center, bounds.radius))
				{
					std::cerr << "SceneVisibilitySmokeTest: visible arena geometry was rejected" << std::endl;
					return false;
				}
				if (!culler.IsSphereVisible(transform.position, 0.8660254f))
					++reproducedLocalBoundsFailures;
			}
		}
	}
	std::cout << "[Visibility] " << visibleCornerCases << " visible-corner cases; " << reproducedLocalBoundsFailures
		<< " would be rejected by unscaled bounds" << std::endl;
	return visibleCornerCases > 0 && reproducedLocalBoundsFailures > 0;
}

bool SceneHierarchyIndexSmokeTest()
{
	Scene scene;
	auto rootA = scene.CreateEntity("RootA");
	rootA->AddComponent<TransformComponent>();
	auto rootB = scene.CreateEntity("RootB");
	rootB->AddComponent<TransformComponent>();
	auto plain = scene.CreateEntity("WithoutTransform");
	auto child = scene.CreateEntity("Child");
	child->AddComponent<TransformComponent>().parentID = rootA->GetID();
	auto grandchild = scene.CreateEntity("Grandchild");
	grandchild->AddComponent<TransformComponent>().parentID = child->GetID();
	auto selfParent = scene.CreateEntity("SelfParent");
	selfParent->AddComponent<TransformComponent>().parentID = selfParent->GetID();
	scene.GetEntities().push_back(nullptr);

	const MyEngine::Editor::SceneHierarchyIndex first(scene);
	if (first.Roots().size() != 3 || first.Roots()[0] != rootA || first.Roots()[1] != rootB || first.Roots()[2] != plain ||
		first.Children(rootA->GetID()).size() != 1 || first.Children(rootA->GetID())[0] != child ||
		first.Children(child->GetID()).size() != 1 || first.Children(child->GetID())[0] != grandchild ||
		!first.Children(selfParent->GetID()).empty() || !first.Children(999999).empty())
		return false;

	if (!TransformHierarchy::SetParent(scene, *child, rootB->GetID()))
		return false;
	const MyEngine::Editor::SceneHierarchyIndex reparented(scene);
	if (!reparented.Children(rootA->GetID()).empty() || reparented.Children(rootB->GetID()).size() != 1 ||
		first.Children(rootA->GetID()).size() != 1)
		return false;
	if (!TransformHierarchy::SetParent(scene, *child, 0))
		return false;
	scene.DestroyEntity(rootA->GetID());
	for (int i = 0; i < 200; ++i)
		scene.CreateEntity("FlatRoot")->AddComponent<TransformComponent>();
	const MyEngine::Editor::SceneHierarchyIndex rebuilt(scene);
	if (rebuilt.Roots().size() != 203 || !rebuilt.Children(rootB->GetID()).empty())
		return false;
	std::set<uint32_t> unique;
	for (const auto& entity : rebuilt.Roots())
		if (!entity || !unique.insert(entity->GetID()).second || entity == rootA)
			return false;
	return first.Roots()[0] == rootA && first.Children(child->GetID())[0] == grandchild;
}
