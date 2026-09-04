#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <iostream>

namespace MyEngine
{
	Texture::Texture(const std::string& path, bool generateMipmaps)
		: m_Path(path)
	{
		glGenTextures(1, &m_TextureID);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);

		// Set texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Set texture filtering parameters
		if (generateMipmaps)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		// Load image using stb_image
		stbi_set_flip_vertically_on_load(true);
		int width, height, nrChannels;
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

		if (data)
		{
			m_Width = static_cast<unsigned int>(width);
			m_Height = static_cast<unsigned int>(height);

			GLenum format = GL_RGB;
			if (nrChannels == 1)
				format = GL_RED;
			else if (nrChannels == 3)
				format = GL_RGB;
			else if (nrChannels == 4)
				format = GL_RGBA;

			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

			if (generateMipmaps)
			{
				glGenerateMipmap(GL_TEXTURE_2D);
			}

			// Sample first few pixels for debugging
			std::cout << "[Texture] Loaded texture: " << path << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
			std::cout << "[Texture] First pixel: ";
			for (int i = 0; i < std::min(nrChannels, 4); ++i)
			{
				std::cout << static_cast<int>(data[i]) << " ";
			}
			std::cout << std::endl;
		}
		else
		{
			std::cerr << "[Texture] Failed to load texture: " << path << std::endl;
			std::cerr << "[STB] " << stbi_failure_reason() << std::endl;
		}

		stbi_image_free(data);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture::Texture(unsigned int width, unsigned int height, GLenum format)
		: m_Width(width), m_Height(height), m_Path("(Generated)")
	{
		glGenTextures(1, &m_TextureID);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture::~Texture()
	{
		Release();
	}

	Texture::Texture(Texture&& other) noexcept
		: m_TextureID(other.m_TextureID)
		, m_Width(other.m_Width)
		, m_Height(other.m_Height)
		, m_Path(std::move(other.m_Path))
	{
		other.m_TextureID = 0;
		other.m_Width = 0;
		other.m_Height = 0;
	}

	Texture& Texture::operator=(Texture&& other) noexcept
	{
		if (this != &other)
		{
			Release();

			m_TextureID = other.m_TextureID;
			m_Width = other.m_Width;
			m_Height = other.m_Height;
			m_Path = std::move(other.m_Path);

			other.m_TextureID = 0;
			other.m_Width = 0;
			other.m_Height = 0;
		}
		return *this;
	}

	void Texture::Bind(unsigned int slot) const
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);
	}

	void Texture::Unbind() const
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Texture::Release()
	{
		if (m_TextureID != 0)
		{
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
	}
}
