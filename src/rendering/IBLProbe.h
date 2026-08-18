#pragma once

#include <glad/glad.h>
#include <memory>

namespace MyEngine
{
	class Shader;

	// Image-Based Lighting probe.
	//
	// Given a loaded skybox cubemap (GL_TEXTURE_CUBE_MAP) it offline-bakes:
	//   - Irradiance map   (32x32  diffuse convolution of the environment)
	//   - Prefilter map    (128x128 mip-mapped GGX specular convolution)
	//   - BRDF LUT         (512x512 2D split-sum approximation look-up table)
	//
	// Call Init(skyboxCubemap) once after the skybox is loaded.
	// Then bind via BindForPBR(firstFreeTextureUnit) before each PBR draw.
	class IBLProbe
	{
	public:
		IBLProbe()  = default;
		~IBLProbe();

		IBLProbe(const IBLProbe&)            = delete;
		IBLProbe& operator=(const IBLProbe&) = delete;

		// Bake all three IBL textures from the supplied equirectangular or
		// cubemap environment. Pass the skybox GL_TEXTURE_CUBE_MAP id.
		// Returns true on success.
		bool Init(GLuint skyboxCubemap);

		bool IsReady() const { return m_Ready; }

		// Bind the three IBL textures starting at `firstUnit`.
		// Units used: firstUnit+0 = irradiance, +1 = prefilter, +2 = BRDF LUT.
		// Returns the next free texture unit (firstUnit + 3).
		int  BindForPBR(int firstUnit) const;

		// Raw texture handles (for imgui previews etc.)
		GLuint GetIrradianceMap()  const { return m_IrradianceMap; }
		GLuint GetPrefilterMap()   const { return m_PrefilterMap;  }
		GLuint GetBRDFLUT()        const { return m_BrdfLUT;       }

	private:
		void Cleanup();
		void BakeIrradiance(GLuint srcCubemap);
		void BakePrefilter(GLuint srcCubemap);
		void BakeBRDFLUT();
		void RenderCube();
		void RenderQuad();

		GLuint m_IrradianceMap = 0;
		GLuint m_PrefilterMap  = 0;
		GLuint m_BrdfLUT       = 0;

		GLuint m_CaptureFBO = 0;
		GLuint m_CaptureRBO = 0;

		// Tiny VAOs for the bake draw calls
		GLuint m_CubeVAO = 0;
		GLuint m_CubeVBO = 0;
		GLuint m_QuadVAO = 0;
		GLuint m_QuadVBO = 0;

		std::shared_ptr<Shader> m_IrradianceShader;
		std::shared_ptr<Shader> m_PrefilterShader;
		std::shared_ptr<Shader> m_BrdfShader;

		bool m_Ready = false;
	};
}
