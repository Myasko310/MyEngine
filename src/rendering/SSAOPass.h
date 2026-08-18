#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace MyEngine
{
	class Shader;

	// Screen-Space Ambient Occlusion pass.
	//
	// Usage per frame:
	//   1. BeginGeometryPass()   — bind the g-buffer FBO and clear it
	//   2. Render all opaque geometry with the g-buffer shader
	//   3. EndGeometryPass()     — unbind g-buffer FBO
	//   4. Compute(projection)   — run SSAO + blur; result in GetOcclusionTexture()
	//   5. Bind GetOcclusionTexture() as u_SSAOTexture in lit/PBR shaders
	class SSAOPass
	{
	public:
		SSAOPass() = default;
		~SSAOPass();

		SSAOPass(const SSAOPass&) = delete;
		SSAOPass& operator=(const SSAOPass&) = delete;

		void Init(unsigned int width, unsigned int height);
		void Resize(unsigned int width, unsigned int height);

		// Bind the g-buffer for writing (geometry pre-pass).
		void BeginGeometryPass() const;
		void EndGeometryPass() const;

		// Run the SSAO occlusion + blur passes.
		// projection is the camera's projection matrix (needed to reconstruct
		// view-space positions from depth).
		void Compute(const glm::mat4& projection, const glm::mat4& view);

		// GL texture handles
		unsigned int GetOcclusionTexture()  const { return m_SSAOBlurTexture; }
		unsigned int GetPositionTexture()   const { return m_GPositionTexture; }
		unsigned int GetNormalTexture()     const { return m_GNormalTexture; }

		// Accessors for tuning parameters
		void  SetRadius(float r)  { m_Radius  = r; }
		float GetRadius()   const { return m_Radius; }

		void  SetBias(float b)    { m_Bias    = b; }
		float GetBias()     const { return m_Bias; }

		void  SetPower(float p)   { m_Power   = p; }
		float GetPower()    const { return m_Power; }

		// Returns the g-buffer shader so MeshRendererSystem can render geometry
		// into the g-buffer with the correct shader.
		Shader* GetGBufferShader() const { return m_GBufferShader.get(); }

	private:
		void CreateResources(unsigned int width, unsigned int height);
		void DestroyResources();
		void GenerateKernel();
		void GenerateNoiseTexture();
		void RenderFullscreenQuad();

	private:
		unsigned int m_Width  = 0;
		unsigned int m_Height = 0;

		// G-buffer: view-space position (RGB16F) + view-space normal (RGB16F)
		unsigned int m_GBufferFBO       = 0;
		unsigned int m_GPositionTexture = 0;
		unsigned int m_GNormalTexture   = 0;
		unsigned int m_GDepthRBO        = 0;

		// SSAO occlusion (R16F), half resolution
		unsigned int m_SSAOFBO          = 0;
		unsigned int m_SSAOTexture      = 0;

		// Blur result (R16F), half resolution
		unsigned int m_SSAOBlurFBO      = 0;
		unsigned int m_SSAOBlurTexture  = 0;

		// 4×4 rotation noise texture
		unsigned int m_NoiseTexture     = 0;

		// Fullscreen quad
		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;

		// Hemisphere sample kernel (64 samples)
		std::vector<glm::vec3> m_Kernel;

		// Shaders
		std::shared_ptr<Shader> m_GBufferShader;
		std::shared_ptr<Shader> m_SSAOShader;
		std::shared_ptr<Shader> m_BlurShader;

		// Parameters
		float m_Radius = 0.5f;
		float m_Bias   = 0.025f;
		float m_Power  = 1.5f;
	};
}
