#pragma once

#include <glad/glad.h>

namespace MyEngine
{
	class ShadowMap
	{
	public:
		ShadowMap() = default;
		~ShadowMap();

		void Init(unsigned int width, unsigned int height);
		void BindForWriting() const;
		void Unbind() const;
		void BindForReading(unsigned int textureUnit) const;

		unsigned int GetDepthTexture() const { return m_DepthMap; }
		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }

	private:
		unsigned int m_FBO = 0;
		unsigned int m_DepthMap = 0;
		unsigned int m_Width = 0;
		unsigned int m_Height = 0;
	};
}
