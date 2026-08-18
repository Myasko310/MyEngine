#include "rendering/SSAOPass.h"
#include "rendering/Shader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

namespace MyEngine
{
	SSAOPass::~SSAOPass()
	{
		DestroyResources();
		if (m_QuadVBO) glDeleteBuffers(1, &m_QuadVBO);
		if (m_QuadVAO) glDeleteVertexArrays(1, &m_QuadVAO);
	}

	void SSAOPass::Init(unsigned int width, unsigned int height)
	{
		// Fullscreen quad (position + texcoords)
		float quadVertices[] = {
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,
		};
		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);

		m_GBufferShader = std::make_shared<Shader>("shaders/gbuffer.vert", "shaders/gbuffer.frag");
		m_SSAOShader    = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/ssao.frag");
		m_BlurShader    = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/ssao_blur.frag");

		GenerateKernel();
		GenerateNoiseTexture();
		CreateResources(width, height);
	}

	void SSAOPass::Resize(unsigned int width, unsigned int height)
	{
		if (width == m_Width && height == m_Height)
			return;
		CreateResources(width, height);
	}

	void SSAOPass::GenerateKernel()
	{
		std::uniform_real_distribution<float> randFloat(0.0f, 1.0f);
		std::default_random_engine gen(42u);

		m_Kernel.clear();
		m_Kernel.reserve(64);
		for (int i = 0; i < 64; ++i)
		{
			glm::vec3 sample(
				randFloat(gen) * 2.0f - 1.0f,
				randFloat(gen) * 2.0f - 1.0f,
				randFloat(gen)
			);
			sample = glm::normalize(sample);
			sample *= randFloat(gen);
			// Accelerating interpolation towards origin
			float scale = static_cast<float>(i) / 64.0f;
			scale = 0.1f + scale * scale * 0.9f;
			m_Kernel.push_back(sample * scale);
		}
	}

	void SSAOPass::GenerateNoiseTexture()
	{
		std::uniform_real_distribution<float> randFloat(0.0f, 1.0f);
		std::default_random_engine gen(123u);

		std::vector<glm::vec3> noiseData(16);
		for (auto& n : noiseData)
			n = glm::vec3(randFloat(gen) * 2.0f - 1.0f, randFloat(gen) * 2.0f - 1.0f, 0.0f);

		if (m_NoiseTexture) glDeleteTextures(1, &m_NoiseTexture);
		glGenTextures(1, &m_NoiseTexture);
		glBindTexture(GL_TEXTURE_2D, m_NoiseTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noiseData.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	void SSAOPass::CreateResources(unsigned int width, unsigned int height)
	{
		DestroyResources();
		m_Width  = width;
		m_Height = height;

		const unsigned int halfW = std::max(1u, width  / 2);
		const unsigned int halfH = std::max(1u, height / 2);

		// --- G-buffer at full resolution ---
		glGenFramebuffers(1, &m_GBufferFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_GBufferFBO);

		// Attachment 0: view-space position
		glGenTextures(1, &m_GPositionTexture);
		glBindTexture(GL_TEXTURE_2D, m_GPositionTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei)width, (GLsizei)height, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_GPositionTexture, 0);

		// Attachment 1: view-space normal
		glGenTextures(1, &m_GNormalTexture);
		glBindTexture(GL_TEXTURE_2D, m_GNormalTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei)width, (GLsizei)height, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_GNormalTexture, 0);

		GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, drawBuffers);

		glGenRenderbuffers(1, &m_GDepthRBO);
		glBindRenderbuffer(GL_RENDERBUFFER, m_GDepthRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)width, (GLsizei)height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_GDepthRBO);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "SSAOPass: g-buffer FBO not complete\n";

		// --- SSAO at half resolution ---
		glGenFramebuffers(1, &m_SSAOFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);
		glGenTextures(1, &m_SSAOTexture);
		glBindTexture(GL_TEXTURE_2D, m_SSAOTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, (GLsizei)halfW, (GLsizei)halfH, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SSAOTexture, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "SSAOPass: SSAO FBO not complete\n";

		// --- Blur at half resolution ---
		glGenFramebuffers(1, &m_SSAOBlurFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOBlurFBO);
		glGenTextures(1, &m_SSAOBlurTexture);
		glBindTexture(GL_TEXTURE_2D, m_SSAOBlurTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, (GLsizei)halfW, (GLsizei)halfH, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SSAOBlurTexture, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "SSAOPass: blur FBO not complete\n";

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SSAOPass::DestroyResources()
	{
		if (m_GPositionTexture) { glDeleteTextures(1, &m_GPositionTexture); m_GPositionTexture = 0; }
		if (m_GNormalTexture)   { glDeleteTextures(1, &m_GNormalTexture);   m_GNormalTexture   = 0; }
		if (m_GDepthRBO)        { glDeleteRenderbuffers(1, &m_GDepthRBO);   m_GDepthRBO        = 0; }
		if (m_GBufferFBO)       { glDeleteFramebuffers(1, &m_GBufferFBO);   m_GBufferFBO       = 0; }
		if (m_SSAOTexture)      { glDeleteTextures(1, &m_SSAOTexture);      m_SSAOTexture      = 0; }
		if (m_SSAOFBO)          { glDeleteFramebuffers(1, &m_SSAOFBO);      m_SSAOFBO          = 0; }
		if (m_SSAOBlurTexture)  { glDeleteTextures(1, &m_SSAOBlurTexture);  m_SSAOBlurTexture  = 0; }
		if (m_SSAOBlurFBO)      { glDeleteFramebuffers(1, &m_SSAOBlurFBO);  m_SSAOBlurFBO      = 0; }
	}

	void SSAOPass::BeginGeometryPass() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_GBufferFBO);
		glViewport(0, 0, (GLsizei)m_Width, (GLsizei)m_Height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void SSAOPass::EndGeometryPass() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SSAOPass::Compute(const glm::mat4& projection, const glm::mat4& view)
	{
		const unsigned int halfW = std::max(1u, m_Width  / 2);
		const unsigned int halfH = std::max(1u, m_Height / 2);

		glDisable(GL_DEPTH_TEST);

		// --- SSAO occlusion pass (half-res) ---
		glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);
		glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
		glClear(GL_COLOR_BUFFER_BIT);

		m_SSAOShader->Use();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_GPositionTexture);
		m_SSAOShader->SetInt("u_Position", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_GNormalTexture);
		m_SSAOShader->SetInt("u_Normal", 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_NoiseTexture);
		m_SSAOShader->SetInt("u_Noise", 2);

		for (int i = 0; i < static_cast<int>(m_Kernel.size()); ++i)
			m_SSAOShader->SetVec3("u_Samples[" + std::to_string(i) + "]", m_Kernel[i]);

		m_SSAOShader->SetMat4("u_Projection", projection);
		m_SSAOShader->SetMat4("u_View", view);
		m_SSAOShader->SetFloat("u_Radius", m_Radius);
		m_SSAOShader->SetFloat("u_Bias",   m_Bias);
		m_SSAOShader->SetFloat("u_Power",  m_Power);
		m_SSAOShader->SetVec2("u_NoiseScale",
			glm::vec2(static_cast<float>(halfW) / 4.0f, static_cast<float>(halfH) / 4.0f));

		RenderFullscreenQuad();

		// --- Blur pass (half-res) ---
		glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOBlurFBO);
		glClear(GL_COLOR_BUFFER_BIT);

		m_BlurShader->Use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_SSAOTexture);
		m_BlurShader->SetInt("u_SSAOInput", 0);

		RenderFullscreenQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glEnable(GL_DEPTH_TEST);
	}

	void SSAOPass::RenderFullscreenQuad()
	{
		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}
}
