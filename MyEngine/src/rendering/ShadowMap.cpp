#include "rendering/ShadowMap.h"

#include <iostream>

namespace MyEngine
{
	ShadowMap::~ShadowMap()
	{
		if (m_DepthMap != 0)
			glDeleteTextures(1, &m_DepthMap);

		if (m_FBO != 0)
			glDeleteFramebuffers(1, &m_FBO);
	}

	void ShadowMap::Init(unsigned int width, unsigned int height)
	{
		m_Width = width;
		m_Height = height;

		if (m_DepthMap != 0)
		{
			glDeleteTextures(1, &m_DepthMap);
			m_DepthMap = 0;
		}

		glGenTextures(1, &m_DepthMap);
		glBindTexture(GL_TEXTURE_2D, m_DepthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_Width, m_Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cerr << "ShadowMap framebuffer not complete" << std::endl;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ShadowMap::BindForWriting() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void ShadowMap::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ShadowMap::BindForReading(unsigned int textureUnit) const
	{
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, m_DepthMap);
	}
}
