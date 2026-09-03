#include "renderer/RenderCommandValidator.h"

#include <cmath>
#include <type_traits>
#include <variant>

namespace MyEngine
{
	namespace
	{
		bool IsFinite(float value)
		{
			return std::isfinite(value);
		}
	}

	RenderCommandValidationResult RenderCommandValidator::Validate(const RenderCommand& command)
	{
		RenderCommandValidationResult result{};

		std::visit([&](const auto& typed)
		{
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, RenderClearCommand>)
			{
				result = ValidateClear(typed);
			}
			else if constexpr (std::is_same_v<T, RenderDrawIndexedCommand>)
			{
				result = ValidateDrawIndexed(typed);
			}
			else
			{
				result = {};
			}
		}, command);

		return result;
	}

	RenderCommandValidationResult RenderCommandValidator::ValidateClear(const RenderClearCommand& command)
	{
		if (!command.clearColor && !command.clearDepth && !command.clearStencil)
			return { false, "clear command targets nothing" };

		if (command.clearColor)
		{
			for (int i = 0; i < 4; ++i)
			{
				if (!IsFinite(command.color[i]))
					return { false, "clear color contains non-finite value" };
				if (command.color[i] < 0.0f || command.color[i] > 1.0f)
					return { false, "clear color channel out of [0,1] range" };
			}
		}

		if (command.clearDepth)
		{
			if (!IsFinite(command.depth))
				return { false, "clear depth is non-finite" };
			if (command.depth < 0.0f || command.depth > 1.0f)
				return { false, "clear depth out of [0,1] range" };
		}

		if (command.clearStencil && command.stencil > 0xFFu)
			return { false, "clear stencil out of 8-bit range" };

		return {};
	}

	RenderCommandValidationResult RenderCommandValidator::ValidateDrawIndexed(const RenderDrawIndexedCommand& command)
	{
		if (command.pipelineHandle == 0)
			return { false, "draw command missing pipeline handle" };
		if (command.meshHandle == 0)
			return { false, "draw command missing mesh handle" };
		if (command.indexCount == 0)
			return { false, "draw command has zero index count" };
		if (command.instanceCount == 0)
			return { false, "draw command has zero instance count" };

		return {};
	}
}
