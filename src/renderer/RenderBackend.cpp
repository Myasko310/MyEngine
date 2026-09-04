#include "renderer/RenderBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace MyEngine
{
	namespace
	{
		std::string ToLowerCopy(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
			return value;
		}
	}

	RenderBackendType RenderBackendSelector::Parse(const std::string& value, bool& valid)
	{
		const std::string normalized = ToLowerCopy(value);
		if (normalized == "opengl" || normalized == "gl")
		{
			valid = true;
			return RenderBackendType::OpenGL;
		}

		if (normalized == "dx12" || normalized == "d3d12" || normalized == "directx12")
		{
			valid = true;
			return RenderBackendType::DirectX12;
		}

		valid = false;
		return RenderBackendType::OpenGL;
	}

	const char* RenderBackendSelector::ToString(RenderBackendType backend)
	{
		switch (backend)
		{
		case RenderBackendType::DirectX12:
			return "DirectX12";
		case RenderBackendType::OpenGL:
		default:
			return "OpenGL";
		}
	}

	RenderBackendCapabilities RenderBackendSelector::GetCapabilities(RenderBackendType backend)
	{
		switch (backend)
		{
		case RenderBackendType::DirectX12:
			return { false, true, true };
		case RenderBackendType::OpenGL:
		default:
			return { true, false, false };
		}
	}

	RenderBackendSelection RenderBackendSelector::Select(int argc, char** argv)
	{
		RenderBackendSelection selection;

		if (const char* env = std::getenv("MYENGINE_RENDER_BACKEND"))
		{
			bool valid = false;
			RenderBackendType parsed = Parse(env, valid);
			if (valid)
			{
				selection.requested = parsed;
				selection.explicitSelection = true;
			}
		}

		for (int i = 1; i < argc; ++i)
		{
			if (!argv[i])
				continue;

			std::string arg = argv[i];
			const std::string key = "--renderer=";
			if (arg.rfind(key, 0) == 0)
			{
				bool valid = false;
				RenderBackendType parsed = Parse(arg.substr(key.size()), valid);
				if (valid)
				{
					selection.requested = parsed;
					selection.explicitSelection = true;
				}
			}
		}

		return selection;
	}
}
