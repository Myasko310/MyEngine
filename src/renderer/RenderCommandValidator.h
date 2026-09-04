#pragma once

#include "renderer/RenderCommand.h"

namespace MyEngine
{
	struct RenderCommandValidationResult
	{
		bool isValid = true;
		const char* reason = nullptr;
	};

	class RenderCommandValidator
	{
	public:
		static RenderCommandValidationResult Validate(const RenderCommand& command);
		static RenderCommandValidationResult ValidateClear(const RenderClearCommand& command);
		static RenderCommandValidationResult ValidateDrawIndexed(const RenderDrawIndexedCommand& command);
	};
}
