#include "rendering/IBLProbe.h"
#include "rendering/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>

namespace MyEngine
{
	// ---------------------------------------------------------------------------
	// Cube geometry (36 vertices, positions only) used for convolution bake passes
	// ---------------------------------------------------------------------------
	static const float s_CubeVertices[] = {
		// back
		-1,-1,-1,  1, 1,-1,  1,-1,-1,  1, 1,-1, -1,-1,-1, -1, 1,-1,
		// front
		-1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
		// left
		-1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
		// right
		 1, 1, 1,  1,-1,-1,  1, 1,-1,  1,-1,-1,  1, 1, 1,  1,-1, 1,
		// bottom
		-1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
		// top
		-1, 1,-1,  1, 1, 1,  1, 1,-1,  1, 1, 1, -1, 1,-1, -1, 1, 1
	};
	static const float s_QuadVertices[] = {
		-1, 1, 0,  0, 1,
		-1,-1, 0,  0, 0,
		 1, 1, 0,  1, 1,
		 1,-1, 0,  1, 0
	};

	// 6 view matrices for rendering into a cubemap face
	static const glm::mat4 s_CaptureViews[6] = {
		glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
		glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0))
	};
	static const glm::mat4 s_CaptureProj =
		glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	// ---------------------------------------------------------------------------
	IBLProbe::~IBLProbe() { Cleanup(); }

	void IBLProbe::Cleanup()
	{
		if (m_IrradianceMap) { glDeleteTextures(1, &m_IrradianceMap); m_IrradianceMap = 0; }
		if (m_PrefilterMap)  { glDeleteTextures(1, &m_PrefilterMap);  m_PrefilterMap  = 0; }
		if (m_BrdfLUT)       { glDeleteTextures(1, &m_BrdfLUT);       m_BrdfLUT       = 0; }
		if (m_CaptureFBO)    { glDeleteFramebuffers(1, &m_CaptureFBO); m_CaptureFBO    = 0; }
		if (m_CaptureRBO)    { glDeleteRenderbuffers(1,&m_CaptureRBO); m_CaptureRBO    = 0; }
		if (m_CubeVAO)       { glDeleteVertexArrays(1, &m_CubeVAO);   m_CubeVAO       = 0; }
		if (m_CubeVBO)       { glDeleteBuffers(1, &m_CubeVBO);        m_CubeVBO       = 0; }
		if (m_QuadVAO)       { glDeleteVertexArrays(1, &m_QuadVAO);   m_QuadVAO       = 0; }
		if (m_QuadVBO)       { glDeleteBuffers(1, &m_QuadVBO);        m_QuadVBO       = 0; }
		m_Ready = false;
	}

	bool IBLProbe::Init(GLuint skyboxCubemap)
	{
		if (!skyboxCubemap) return false;
		Cleanup();

		// Load bake shaders
		m_IrradianceShader = std::make_shared<Shader>(
			"shaders/ibl_cubemap.vert", "shaders/ibl_irradiance.frag");
		m_PrefilterShader  = std::make_shared<Shader>(
			"shaders/ibl_cubemap.vert", "shaders/ibl_prefilter.frag");
		m_BrdfShader       = std::make_shared<Shader>(
			"shaders/ibl_brdf.vert",    "shaders/ibl_brdf.frag");

		// Capture framebuffer
		glGenFramebuffers(1,  &m_CaptureFBO);
		glGenRenderbuffers(1, &m_CaptureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);

		BakeIrradiance(skyboxCubemap);
		BakePrefilter(skyboxCubemap);
		BakeBRDFLUT();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		m_Ready = true;
		std::cout << "[IBLProbe] Baked irradiance, prefilter, BRDF LUT.\n";
		return true;
	}

	// ---------------------------------------------------------------------------
	void IBLProbe::BakeIrradiance(GLuint srcCubemap)
	{
		constexpr int SIZE = 32;
		glGenTextures(1, &m_IrradianceMap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_IrradianceMap);
		for (int i = 0; i < 6; ++i)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
				SIZE, SIZE, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SIZE, SIZE);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_RENDERBUFFER, m_CaptureRBO);

		m_IrradianceShader->Use();
		m_IrradianceShader->SetInt("u_EnvironmentMap", 0);
		m_IrradianceShader->SetMat4("u_Projection", s_CaptureProj);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, srcCubemap);

		glViewport(0, 0, SIZE, SIZE);
		for (int i = 0; i < 6; ++i)
		{
			m_IrradianceShader->SetMat4("u_View", s_CaptureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_IrradianceMap, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			RenderCube();
		}
	}

	void IBLProbe::BakePrefilter(GLuint srcCubemap)
	{
		constexpr int SIZE      = 128;
		constexpr int MIP_LEVELS = 5;

		glGenTextures(1, &m_PrefilterMap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_PrefilterMap);
		for (int i = 0; i < 6; ++i)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
				SIZE, SIZE, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		m_PrefilterShader->Use();
		m_PrefilterShader->SetInt("u_EnvironmentMap", 0);
		m_PrefilterShader->SetMat4("u_Projection", s_CaptureProj);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, srcCubemap);

		for (int mip = 0; mip < MIP_LEVELS; ++mip)
		{
			int mipSize = static_cast<int>(SIZE * std::pow(0.5f, mip));
			glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
			glViewport(0, 0, mipSize, mipSize);

			float roughness = static_cast<float>(mip) / static_cast<float>(MIP_LEVELS - 1);
			m_PrefilterShader->SetFloat("u_Roughness", roughness);

			for (int i = 0; i < 6; ++i)
			{
				m_PrefilterShader->SetMat4("u_View", s_CaptureViews[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_PrefilterMap, mip);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				RenderCube();
			}
		}
	}

	void IBLProbe::BakeBRDFLUT()
	{
		constexpr int SIZE = 512;
		glGenTextures(1, &m_BrdfLUT);
		glBindTexture(GL_TEXTURE_2D, m_BrdfLUT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, SIZE, SIZE, 0, GL_RG, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SIZE, SIZE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_BrdfLUT, 0);

		glViewport(0, 0, SIZE, SIZE);
		m_BrdfShader->Use();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RenderQuad();
	}

	// ---------------------------------------------------------------------------
	int IBLProbe::BindForPBR(int firstUnit) const
	{
		glActiveTexture(GL_TEXTURE0 + firstUnit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_IrradianceMap);

		glActiveTexture(GL_TEXTURE0 + firstUnit + 1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_PrefilterMap);

		glActiveTexture(GL_TEXTURE0 + firstUnit + 2);
		glBindTexture(GL_TEXTURE_2D, m_BrdfLUT);

		return firstUnit + 3;
	}

	// ---------------------------------------------------------------------------
	void IBLProbe::RenderCube()
	{
		if (!m_CubeVAO)
		{
			glGenVertexArrays(1, &m_CubeVAO);
			glGenBuffers(1, &m_CubeVBO);
			glBindVertexArray(m_CubeVAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(s_CubeVertices), s_CubeVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		}
		glBindVertexArray(m_CubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}

	void IBLProbe::RenderQuad()
	{
		if (!m_QuadVAO)
		{
			glGenVertexArrays(1, &m_QuadVAO);
			glGenBuffers(1, &m_QuadVBO);
			glBindVertexArray(m_QuadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(s_QuadVertices), s_QuadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		}
		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}
}
