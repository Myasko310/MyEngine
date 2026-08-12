#include "rendering/PostProcessPipeline.h"

#include "rendering/Shader.h"

#include <iostream>

namespace MyEngine
{
	PostProcessPipeline::~PostProcessPipeline()
	{
		DestroyFramebuffers();

		if (m_QuadVBO != 0)
			glDeleteBuffers(1, &m_QuadVBO);
		if (m_QuadVAO != 0)
			glDeleteVertexArrays(1, &m_QuadVAO);
	}

	void PostProcessPipeline::Init(unsigned int width, unsigned int height)
	{
		m_BrightPassShader = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/brightpass.frag");
		m_BlurShader = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/blur.frag");
		m_CompositeShader = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/composite.frag");

		// Fullscreen triangle/quad (position + texcoords)
		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
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

		CreateFramebuffers(width, height);
	}

	void PostProcessPipeline::Resize(unsigned int width, unsigned int height)
	{
		if (width == m_Width && height == m_Height)
			return;

		CreateFramebuffers(width, height);
	}

	void PostProcessPipeline::DestroyFramebuffers()
	{
		if (m_SceneColorTexture != 0)
		{
			glDeleteTextures(1, &m_SceneColorTexture);
			m_SceneColorTexture = 0;
		}
		if (m_DepthRBO != 0)
		{
			glDeleteRenderbuffers(1, &m_DepthRBO);
			m_DepthRBO = 0;
		}
		if (m_HDRFBO != 0)
		{
			glDeleteFramebuffers(1, &m_HDRFBO);
			m_HDRFBO = 0;
		}

		for (int i = 0; i < 2; ++i)
		{
			if (m_PingPongTextures[i] != 0)
			{
				glDeleteTextures(1, &m_PingPongTextures[i]);
				m_PingPongTextures[i] = 0;
			}
			if (m_PingPongFBO[i] != 0)
			{
				glDeleteFramebuffers(1, &m_PingPongFBO[i]);
				m_PingPongFBO[i] = 0;
			}
		}
	}

	void PostProcessPipeline::CreateFramebuffers(unsigned int width, unsigned int height)
	{
		if (width == 0 || height == 0)
			return;

		DestroyFramebuffers();

		m_Width = width;
		m_Height = height;

		// HDR scene color target (16-bit float) + depth renderbuffer
		glGenFramebuffers(1, &m_HDRFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFBO);

		glGenTextures(1, &m_SceneColorTexture);
		glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneColorTexture, 0);

		glGenRenderbuffers(1, &m_DepthRBO);
		glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthRBO);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "PostProcessPipeline: HDR framebuffer not complete" << std::endl;

		// Ping-pong framebuffers for blur (half-res would be an option, but
		// keep same resolution for simplicity/quality).
		glGenFramebuffers(2, m_PingPongFBO);
		glGenTextures(2, m_PingPongTextures);
		for (int i = 0; i < 2; ++i)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFBO[i]);
			glBindTexture(GL_TEXTURE_2D, m_PingPongTextures[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_PingPongTextures[i], 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "PostProcessPipeline: ping-pong framebuffer " << i << " not complete" << std::endl;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostProcessPipeline::BindForWriting() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFBO);
		glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
	}

	void PostProcessPipeline::RenderFullscreenQuad()
	{
		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	void PostProcessPipeline::Composite()
	{
		glDisable(GL_DEPTH_TEST);

		unsigned int bloomResultTexture = 0;

		if (m_BloomEnabled)
		{
			// 1) Bright-pass extraction into ping-pong texture 0
			glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFBO[0]);
			m_BrightPassShader->Use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
			m_BrightPassShader->SetInt("u_SceneTexture", 0);
			m_BrightPassShader->SetFloat("u_Threshold", m_BloomThreshold);
			RenderFullscreenQuad();

			// 2) Ping-pong Gaussian blur (separable, N iterations)
			bool horizontal = true;
			const int blurIterations = 10;
			m_BlurShader->Use();
			for (int i = 0; i < blurIterations; ++i)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_PingPongFBO[horizontal ? 1 : 0]);
				m_BlurShader->SetBool("u_Horizontal", horizontal);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, m_PingPongTextures[horizontal ? 0 : 1]);
				m_BlurShader->SetInt("u_Image", 0);
				RenderFullscreenQuad();
				horizontal = !horizontal;
			}

			// Last written target holds the final blurred bloom result.
			bloomResultTexture = m_PingPongTextures[horizontal ? 0 : 1];
		}

		// 3) Final composite: tone mapping + gamma correction (+ bloom add)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		m_CompositeShader->Use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_SceneColorTexture);
		m_CompositeShader->SetInt("u_SceneTexture", 0);
		m_CompositeShader->SetBool("u_BloomEnabled", m_BloomEnabled);
		if (m_BloomEnabled)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, bloomResultTexture);
			m_CompositeShader->SetInt("u_BloomTexture", 1);
		}
		m_CompositeShader->SetFloat("u_Exposure", m_Exposure);
		m_CompositeShader->SetFloat("u_BloomIntensity", m_BloomIntensity);
		RenderFullscreenQuad();

		glEnable(GL_DEPTH_TEST);
	}
}
