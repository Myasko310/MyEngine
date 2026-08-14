#include "rendering/PointShadowMap.h"

#include <iostream>

namespace MyEngine
{
	PointShadowMap::~PointShadowMap()
	{
		if (m_DepthCubemap != 0)
			glDeleteTextures(1, &m_DepthCubemap);

		if (m_FBO != 0)
			glDeleteFramebuffers(1, &m_FBO);
	}

	void PointShadowMap::Init(unsigned int size)
	{
		m_Size = size;

		if (m_DepthCubemap != 0)
		{
			glDeleteTextures(1, &m_DepthCubemap);
			m_DepthCubemap = 0;
		}

		if (m_FBO == 0)
			glGenFramebuffers(1, &m_FBO);

		glGenTextures(1, &m_DepthCubemap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_DepthCubemap);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
				static_cast<GLsizei>(m_Size), static_cast<GLsizei>(m_Size), 0,
				GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_DepthCubemap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cerr << "PointShadowMap framebuffer not complete" << std::endl;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	void PointShadowMap::BindForWriting() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, static_cast<GLsizei>(m_Size), static_cast<GLsizei>(m_Size));
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void PointShadowMap::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PointShadowMap::BindForReading(unsigned int textureUnit) const
	{
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_DepthCubemap);
	}
}
