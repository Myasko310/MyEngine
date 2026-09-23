#include "rendering/Skybox.h"

#include "rendering/Shader.h"

#include <stb_image.h>

#include <iostream>

namespace MyEngine
{
	namespace
	{
		// Unit cube positions only (no normals/texcoords needed - the
		// direction vector used to sample the cubemap is just the vertex
		// position itself).
		constexpr float kSkyboxVertices[] = {
			// positions
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};
	}

	Skybox::~Skybox()
	{
		if (m_CubemapTexture != 0)
			glDeleteTextures(1, &m_CubemapTexture);
		if (m_VBO != 0)
			glDeleteBuffers(1, &m_VBO);
		if (m_VAO != 0)
			glDeleteVertexArrays(1, &m_VAO);
	}

	void Skybox::EnsureGeometry()
	{
		if (m_VAO != 0)
			return;

		glGenVertexArrays(1, &m_VAO);
		glGenBuffers(1, &m_VBO);
		glBindVertexArray(m_VAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices), kSkyboxVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glBindVertexArray(0);
	}

	bool Skybox::Load(const std::array<std::string, 6>& facePaths)
	{
		EnsureGeometry();

		if (!m_Shader)
			m_Shader = std::make_shared<Shader>("shaders/skybox.vert", "shaders/skybox.frag");

		unsigned int newTexture = 0;
		glGenTextures(1, &newTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, newTexture);

		// Cubemap face images typically should NOT be vertically flipped
		// (unlike regular 2D textures), otherwise the faces are mismatched.
		stbi_set_flip_vertically_on_load(false);

		bool allFacesLoaded = true;
		int expectedWidth = 0;
		int expectedHeight = 0;
		for (size_t i = 0; i < facePaths.size(); ++i)
		{
			int width = 0, height = 0, channels = 0;
			// Force 4 channels so all cubemap faces use the same format.
			unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
			if (!data)
			{
				std::cerr << "[Skybox] Failed to load face " << i << " (" << facePaths[i] << "): "
					<< stbi_failure_reason() << std::endl;
				allFacesLoaded = false;
				break;
			}

			if (i == 0)
			{
				expectedWidth = width;
				expectedHeight = height;
			}
			else if (width != expectedWidth || height != expectedHeight)
			{
				std::cerr << "[Skybox] Face size mismatch at face " << i << " (" << facePaths[i] << "): "
					<< width << "x" << height << " does not match first face "
					<< expectedWidth << "x" << expectedHeight << std::endl;
				stbi_image_free(data);
				allFacesLoaded = false;
				break;
			}

			glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, GL_RGBA,
				width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
		}

		if (!allFacesLoaded)
		{
			std::cerr << "[Skybox] Cubemap load aborted: all 6 faces are required and must share identical dimensions." << std::endl;
			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			glDeleteTextures(1, &newTexture);
			// Restore default behavior used by regular 2D texture loading.
			stbi_set_flip_vertically_on_load(true);
			return false;
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		if (m_CubemapTexture != 0)
			glDeleteTextures(1, &m_CubemapTexture);
		m_CubemapTexture = newTexture;

		// Restore default behavior used by regular 2D texture loading.
		stbi_set_flip_vertically_on_load(true);
		return true;
	}

	bool Skybox::LoadSunset()
	{
		Shader bake("shaders/sunset_bake.vert", "shaders/sunset_bake.frag");
		if (bake.GetID() == 0 || !bake.GetLastError().empty())
			return false;
		if (!m_Shader)
			m_Shader = std::make_shared<Shader>("shaders/skybox.vert", "shaders/skybox.frag");
		if (m_Shader->GetID() == 0 || !m_Shader->GetLastError().empty())
			return false;

		GLint previousDrawFBO, previousReadFBO, previousViewport[4], previousProgram, previousVAO, previousCube, previousBuffer;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFBO);
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFBO);
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
		glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCube);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
		const GLenum capabilities[] = { GL_DEPTH_TEST, GL_CULL_FACE, GL_BLEND, GL_SCISSOR_TEST, GL_FRAMEBUFFER_SRGB };
		GLboolean enabled[5];
		for (int i = 0; i < 5; ++i)
		{
			enabled[i] = glIsEnabled(capabilities[i]);
			glDisable(capabilities[i]);
		}
		GLboolean colorMask[4];
		glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		constexpr int resolution = 512;
		GLuint texture = 0, framebuffer = 0, vao = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
		for (int face = 0; face < 6; ++face)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F,
				resolution, resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glViewport(0, 0, resolution, resolution);
		bake.Use();
		bake.SetFloat("u_Resolution", static_cast<float>(resolution));
		bool success = true;
		for (int face = 0; face < 6; ++face)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				success = false;
				break;
			}
			bake.SetInt("u_Face", face);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFBO);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFBO);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
		glUseProgram(previousProgram);
		glBindVertexArray(previousVAO);
		glBindTexture(GL_TEXTURE_CUBE_MAP, previousCube);
		glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
		for (int i = 0; i < 5; ++i)
			if (enabled[i]) glEnable(capabilities[i]);
		glDeleteFramebuffers(1, &framebuffer);
		glDeleteVertexArrays(1, &vao);
		if (!success)
		{
			glDeleteTextures(1, &texture);
			return false;
		}
		EnsureGeometry();
		glBindVertexArray(previousVAO);
		glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
		if (m_CubemapTexture != 0)
			glDeleteTextures(1, &m_CubemapTexture);
		m_CubemapTexture = texture;
		return true;
	}

	void Skybox::Render(const glm::mat4& view, const glm::mat4& projection)
	{
		if (m_CubemapTexture == 0 || !m_Shader)
			return;

		// Strip translation from the view matrix so the skybox always
		// appears infinitely far away and centered on the camera.
		glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));

		// Save GL state/bindings touched by the skybox pass so they cannot
		// leak into other scene draws.
		GLint previousDepthFunc = GL_LESS;
		glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
		GLboolean depthMaskEnabled = GL_TRUE;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);
		GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
		GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
		GLint previousActiveTexture = GL_TEXTURE0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
		GLint previousProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
		GLint previousVAO = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
		glActiveTexture(GL_TEXTURE0);
		GLint previousCubeMapBinding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCubeMapBinding);
		GLboolean seamlessEnabled = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		// Depth function LEQUAL is required because the skybox is drawn
		// with a depth value of exactly 1.0 (far plane) via the shader's
		// gl_Position.z = w trick, and the default depth buffer clear
		// value is also 1.0, so GL_LESS would fail the depth test entirely.
		if (!depthTestEnabled)
			glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);

		m_Shader->Use();
		m_Shader->SetMat4("u_View", viewNoTranslation);
		m_Shader->SetMat4("u_Projection", projection);
		m_Shader->SetInt("u_Skybox", 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubemapTexture);

		glBindVertexArray(m_VAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Restore modified state.
		glBindVertexArray(static_cast<GLuint>(previousVAO));
		glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previousCubeMapBinding));
		glUseProgram(static_cast<GLuint>(previousProgram));
		glActiveTexture(static_cast<GLenum>(previousActiveTexture));
		if (!seamlessEnabled)
			glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		if (cullFaceEnabled)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		if (depthTestEnabled)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		glDepthMask(depthMaskEnabled);
		glDepthFunc(previousDepthFunc);
	}
}
