#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "rendering/Skybox.h"
#include "serialization/SceneSerializer.h"
#include <rapidjson/document.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cmath>
#include <vector>
#include <iostream>

bool SunsetSkyboxSmokeTest()
{
	Scene scene;
	scene.sunsetSkyboxEnabled = true;
	scene.CreateEntity("Preserved");
	const auto saved = MyEngine::Serialization::SaveSceneToString(scene);
	Scene restored;
	if (!MyEngine::Serialization::LoadSceneFromString(restored, saved) || !restored.sunsetSkyboxEnabled || restored.GetEntities().size() != 1)
		return false;
	if (!MyEngine::Serialization::LoadSceneFromString(restored, "{\"entities\":[]}") || restored.sunsetSkyboxEnabled)
		return false;
	restored.sunsetSkyboxEnabled = true;
	restored.Clear();
	if (restored.sunsetSkyboxEnabled)
		return false;

	const auto root = std::filesystem::path(MYENGINE_TEST_SOURCE_DIR);
	std::ifstream input(root / "assets/models/akaza animations/akaza player2.json");
	const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	rapidjson::Document doc;
	if (doc.Parse(json.c_str()).HasParseError() || !doc.HasMember("sunsetSkyboxEnabled") || !doc["sunsetSkyboxEnabled"].GetBool())
		return false;

	if (!glfwInit()) return false;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(32, 32, "SunsetSkyboxSmokeTest", nullptr, nullptr);
	if (!window) { glfwTerminate(); return false; }
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	const auto previousDirectory = std::filesystem::current_path();
	std::filesystem::current_path(root);
	bool passed = true;
	{
		MyEngine::Skybox sky;
		glViewport(2, 3, 28, 27);
		glEnable(GL_SCISSOR_TEST);
		glEnable(GL_BLEND);
		glActiveTexture(GL_TEXTURE3);
		passed = sky.LoadSunset() && sky.IsLoaded();
		GLint viewport[4], active;
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		passed = passed && viewport[0] == 2 && viewport[1] == 3 && viewport[2] == 28 && viewport[3] == 27
			&& active == GL_TEXTURE3 && glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_BLEND);
		if (sky.IsLoaded())
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP, sky.GetCubemapTexture());
			for (int face = 0; face < 6; ++face)
			{
				GLint width = 0, height = 0, format = 0;
				glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_WIDTH, &width);
				glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_HEIGHT, &height);
				glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
				passed = passed && width == 512 && height == 512 && format == GL_RGBA16F;
				if (width != 512 || height != 512) continue;
				std::vector<float> pixels(width * height * 4);
				glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, GL_FLOAT, pixels.data());
				float minValue = 1000, maxValue = 0;
				for (size_t i = 0; i < pixels.size(); i += 4)
				{
					for (int c = 0; c < 3; ++c)
					{
						passed = passed && std::isfinite(pixels[i + c]) && pixels[i + c] >= 0;
						minValue = std::min(minValue, pixels[i + c]);
						maxValue = std::max(maxValue, pixels[i + c]);
					}
					passed = passed && pixels[i + 3] == 1.0f;
				}
				passed = passed && maxValue - minValue > 0.01f;
			}
			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_BLEND);
			glViewport(0, 0, 32, 32);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			sky.Render(glm::mat4(1), glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f));
			unsigned char pixel[4] = {};
			glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
			passed = passed && (pixel[0] || pixel[1] || pixel[2]) && active == GL_TEXTURE3;
		}
		passed = passed && glGetError() == GL_NO_ERROR;
	}
	std::filesystem::current_path(previousDirectory);
	glfwDestroyWindow(window);
	glfwTerminate();
	if (!passed) std::cerr << "Sunset cubemap rendering/state validation failed" << std::endl;
	return passed;
}
