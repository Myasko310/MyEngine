#pragma once

#include <string>
#include <glad/glad.h>

namespace MyEngine
{
	class Texture
	{
	public:
		// Load texture from file
		Texture(const std::string& path, bool generateMipmaps = true);

		// Create empty texture (for render targets, etc.)
		Texture(unsigned int width, unsigned int height, GLenum format = GL_RGBA);

		~Texture();

		// Non-copyable
		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		// Movable
		Texture(Texture&& other) noexcept;
		Texture& operator=(Texture&& other) noexcept;

		void Bind(unsigned int slot = 0) const;
		void Unbind() const;

		unsigned int GetID() const { return m_TextureID; }
		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }
		const std::string& GetPath() const { return m_Path; }

	private:
		unsigned int m_TextureID = 0;
		unsigned int m_Width = 0;
		unsigned int m_Height = 0;
		std::string m_Path;

		void Release();
	};
}
