#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <filesystem>

void CreateCheckerboard(const std::string& filename, int width, int height, int checkerSize = 32)
{
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int checkX = x / checkerSize;
			int checkY = y / checkerSize;
			bool isWhite = (checkX + checkY) % 2 == 0;

			int idx = (y * width + x) * 3;
			pixels[idx + 0] = isWhite ? 255 : 32;
			pixels[idx + 1] = isWhite ? 255 : 32;
			pixels[idx + 2] = isWhite ? 255 : 32;
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

void CreateGrid(const std::string& filename, int width, int height, int gridSize = 64)
{
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			bool isGridLine = (x % gridSize == 0) || (y % gridSize == 0);

			int idx = (y * width + x) * 3;
			pixels[idx + 0] = isGridLine ? 0 : 200;
			pixels[idx + 1] = isGridLine ? 0 : 200;
			pixels[idx + 2] = isGridLine ? 0 : 200;
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

void CreateGradient(const std::string& filename, int width, int height)
{
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			float u = static_cast<float>(x) / width;
			float v = static_cast<float>(y) / height;

			int idx = (y * width + x) * 3;
			pixels[idx + 0] = static_cast<unsigned char>(u * 255);
			pixels[idx + 1] = static_cast<unsigned char>(v * 255);
			pixels[idx + 2] = static_cast<unsigned char>((1.0f - u) * 255);
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

void CreateBrickWall(const std::string& filename, int width, int height)
{
	std::vector<unsigned char> pixels(width * height * 3);

	int brickWidth = 64;
	int brickHeight = 32;
	int mortarSize = 4;

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int row = y / brickHeight;
			int col = x / brickWidth;

			// Offset every other row
			int offsetX = (row % 2) * (brickWidth / 2);
			int adjustedX = (x + offsetX) % (width + brickWidth);
			int adjustedCol = adjustedX / brickWidth;

			int localX = adjustedX % brickWidth;
			int localY = y % brickHeight;

			bool isMortar = (localX < mortarSize) || (localY < mortarSize);

			int idx = (y * width + x) * 3;
			if (isMortar)
			{
				// Gray mortar
				pixels[idx + 0] = 180;
				pixels[idx + 1] = 180;
				pixels[idx + 2] = 180;
			}
			else
			{
				// Red-brown brick with some variation
				int variation = ((adjustedCol + row) * 17) % 40 - 20;
				pixels[idx + 0] = static_cast<unsigned char>(std::clamp(160 + variation, 0, 255));
				pixels[idx + 1] = static_cast<unsigned char>(std::clamp(80 + variation, 0, 255));
				pixels[idx + 2] = static_cast<unsigned char>(std::clamp(40 + variation, 0, 255));
			}
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

void CreateColoredSquares(const std::string& filename, int width, int height)
{
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int quadX = x / (width / 2);
			int quadY = y / (height / 2);

			int idx = (y * width + x) * 3;

			if (quadX == 0 && quadY == 0)
			{
				// Top-left: Red
				pixels[idx + 0] = 255;
				pixels[idx + 1] = 0;
				pixels[idx + 2] = 0;
			}
			else if (quadX == 1 && quadY == 0)
			{
				// Top-right: Green
				pixels[idx + 0] = 0;
				pixels[idx + 1] = 255;
				pixels[idx + 2] = 0;
			}
			else if (quadX == 0 && quadY == 1)
			{
				// Bottom-left: Blue
				pixels[idx + 0] = 0;
				pixels[idx + 1] = 0;
				pixels[idx + 2] = 255;
			}
			else
			{
				// Bottom-right: Yellow
				pixels[idx + 0] = 255;
				pixels[idx + 1] = 255;
				pixels[idx + 2] = 0;
			}
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

void CreateUVTest(const std::string& filename, int width, int height)
{
	std::vector<unsigned char> pixels(width * height * 3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			float u = static_cast<float>(x) / width;
			float v = static_cast<float>(y) / height;

			int idx = (y * width + x) * 3;
			pixels[idx + 0] = static_cast<unsigned char>(u * 255);  // Red = U
			pixels[idx + 1] = static_cast<unsigned char>(v * 255);  // Green = V
			pixels[idx + 2] = 0;  // Blue = 0
		}
	}

	stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
	std::cout << "Created: " << filename << std::endl;
}

int main()
{
	std::cout << "Generating test textures..." << std::endl;

	// Create assets/textures directory
	std::filesystem::create_directories("assets/textures");

	int size = 512;

	CreateCheckerboard("assets/textures/checkerboard.png", size, size, 32);
	CreateGrid("assets/textures/grid.png", size, size, 64);
	CreateGradient("assets/textures/gradient.png", size, size);
	CreateBrickWall("assets/textures/brickwall.png", size, size);
	CreateColoredSquares("assets/textures/colors.png", size, size);
	CreateUVTest("assets/textures/uvtest.png", size, size);

	std::cout << "\nAll textures generated successfully!" << std::endl;
	std::cout << "Textures saved to: assets/textures/" << std::endl;

	return 0;
}
