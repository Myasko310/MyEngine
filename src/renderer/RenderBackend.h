#pragma once

#include <string>

namespace MyEngine
{
	enum class RenderBackendType
	{
		OpenGL,
		DirectX12
	};

	struct RenderBackendSelection
	{
		RenderBackendType requested = RenderBackendType::OpenGL;
		bool explicitSelection = false;
	};

	struct RenderBackendCapabilities
	{
		bool supportsOpenGLPipeline = false;
		bool supportsNativeDebugCapture = false;
		bool supportsShaderModel6 = false;
	};

	class RenderBackendSelector
	{
	public:
		static RenderBackendSelection Select(int argc, char** argv);
		static RenderBackendType Parse(const std::string& value, bool& valid);
		static const char* ToString(RenderBackendType backend);
		static RenderBackendCapabilities GetCapabilities(RenderBackendType backend);
	};
}
