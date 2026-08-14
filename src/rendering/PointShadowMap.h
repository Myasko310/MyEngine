#pragma once

#include <glad/glad.h>

namespace MyEngine
{
	// Depth cubemap shadow target used by point lights.
	class PointShadowMap
	{
	public:
		PointShadowMap() = default;
		~PointShadowMap();

		void Init(unsigned int size);
		void BindForWriting() const;
		void Unbind() const;
		void BindForReading(unsigned int textureUnit) const;

		unsigned int GetDepthCubemap() const { return m_DepthCubemap; }
		unsigned int GetSize() const { return m_Size; }

	private:
		unsigned int m_FBO = 0;
		unsigned int m_DepthCubemap = 0;
		unsigned int m_Size = 0;
	};
}
