#pragma once

#include <array>
#include <memory>
#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace MyEngine
{
	class Shader;

	// Renders a cubemap-based skybox as the background of the scene. Loads
	// six face images (+X, -X, +Y, -Y, +Z, -Z order, matching OpenGL's
	// GL_TEXTURE_CUBE_MAP_POSITIVE_X.. face enum ordering) into a single
	// GL_TEXTURE_CUBE_MAP and draws it on a unit cube with depth writes
	// disabled and depth test set to GL_LEQUAL so it only shows where no
	// closer geometry has been drawn.
	class Skybox
	{
	public:
		Skybox() = default;
		~Skybox();

		Skybox(const Skybox&) = delete;
		Skybox& operator=(const Skybox&) = delete;

		// Loads the six cubemap face images. Faces must be supplied in the
		// order: +X (right), -X (left), +Y (top), -Y (bottom), +Z (front),
		// -Z (back). Returns true if the cubemap texture was created
		// (individual missing/failed faces are logged but do not abort the
		// whole load, so a partially-broken skybox still renders instead of
		// leaving nothing initialized).
		bool Load(const std::array<std::string, 6>& facePaths);

		// True once a cubemap texture has been successfully created via Load().
		bool IsLoaded() const { return m_CubemapTexture != 0; }

		// Draws the skybox using `view` (rotation-only, translation stripped
		// by the caller or internally) and `projection`. Assumes the caller
		// has already bound the target framebuffer/viewport for the main
		// scene pass.
		void Render(const glm::mat4& view, const glm::mat4& projection);

		unsigned int GetCubemapTexture() const { return m_CubemapTexture; }

	private:
		void EnsureGeometry();

	private:
		unsigned int m_CubemapTexture = 0;
		unsigned int m_VAO = 0;
		unsigned int m_VBO = 0;
		std::shared_ptr<Shader> m_Shader;
	};
}
