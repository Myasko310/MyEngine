#pragma once

#include <cstdint>
#include <variant>

namespace MyEngine
{
	struct RenderBeginFrameCommand
	{
	};

	struct RenderEndFrameCommand
	{
	};

	struct RenderClearCommand
	{
		float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float depth = 1.0f;
		unsigned int stencil = 0;
		bool clearColor = true;
		bool clearDepth = true;
		bool clearStencil = false;
	};

	struct RenderDrawIndexedCommand
	{
		std::uint64_t pipelineHandle = 0;
		std::uint64_t meshHandle = 0;
		std::uint32_t indexCount = 0;
		std::uint32_t startIndex = 0;
		std::int32_t baseVertex = 0;
		std::uint32_t instanceCount = 1;
		std::uint32_t startInstance = 0;
	};

	using RenderCommand = std::variant<
		RenderBeginFrameCommand,
		RenderClearCommand,
		RenderDrawIndexedCommand,
		RenderEndFrameCommand>;
}
