#pragma once

#include <glad/glad.h>
#include <memory>

namespace MyEngine
{
	class Shader;

	// HDR post-processing pipeline: renders the scene into a floating-point
	// HDR framebuffer, extracts bright regions for bloom, blurs them with a
	// ping-pong Gaussian blur, then composites everything back with
	// tone mapping + gamma correction onto the default framebuffer.
	class PostProcessPipeline
	{
	public:
		PostProcessPipeline() = default;
		~PostProcessPipeline();

		PostProcessPipeline(const PostProcessPipeline&) = delete;
		PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

		void Init(unsigned int width, unsigned int height);
		void Resize(unsigned int width, unsigned int height);

		// Bind the HDR scene framebuffer for writing. Call before rendering
		// the scene, then call Composite() afterwards to draw the final
		// tone-mapped/bloomed result to the currently bound (default) target.
		void BindForWriting() const;

		// Runs the bright-pass extraction, blur passes, and final composite
		// to whatever framebuffer is currently bound (typically the default
		// framebuffer, i.e. call glBindFramebuffer(GL_FRAMEBUFFER, 0) first).
		void Composite();

		void SetBloomEnabled(bool enabled) { m_BloomEnabled = enabled; }
		bool GetBloomEnabled() const { return m_BloomEnabled; }

		void SetExposure(float exposure) { m_Exposure = exposure; }
		float GetExposure() const { return m_Exposure; }

		void SetBloomThreshold(float threshold) { m_BloomThreshold = threshold; }
		float GetBloomThreshold() const { return m_BloomThreshold; }

		void SetBloomIntensity(float intensity) { m_BloomIntensity = intensity; }
		float GetBloomIntensity() const { return m_BloomIntensity; }

		unsigned int GetSceneTexture() const { return m_SceneColorTexture; }
		unsigned int GetBloomTexture() const { return m_PingPongTextures[0]; }

	private:
		void CreateFramebuffers(unsigned int width, unsigned int height);
		void DestroyFramebuffers();
		void RenderFullscreenQuad();

	private:
		unsigned int m_Width = 0;
		unsigned int m_Height = 0;

		// Bloom/blur is done at a reduced resolution to avoid the heavy cost
		// of running many full-resolution ping-pong blur passes every frame.
		unsigned int m_BlurWidth = 0;
		unsigned int m_BlurHeight = 0;

		// Main HDR scene target
		unsigned int m_HDRFBO = 0;
		unsigned int m_SceneColorTexture = 0;
		unsigned int m_DepthRBO = 0;

		// Ping-pong framebuffers used for Gaussian blur of bright regions
		// (sized at m_BlurWidth x m_BlurHeight, not the full window resolution).
		unsigned int m_PingPongFBO[2] = { 0, 0 };
		unsigned int m_PingPongTextures[2] = { 0, 0 };

		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;

		std::shared_ptr<Shader> m_BrightPassShader;
		std::shared_ptr<Shader> m_BlurShader;
		std::shared_ptr<Shader> m_CompositeShader;

		bool m_BloomEnabled = true;
		float m_Exposure = 1.0f;
		float m_BloomThreshold = 1.0f;
		float m_BloomIntensity = 0.6f;
	};
}
