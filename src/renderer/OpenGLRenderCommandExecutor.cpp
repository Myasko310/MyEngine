#include "renderer/OpenGLRenderCommandExecutor.h"

#include "renderer/RenderCommandValidator.h"

#include <glad/glad.h>

#include <type_traits>
#include <variant>

namespace MyEngine
{
	void OpenGLRenderCommandExecutor::Execute(const RenderCommandList& commandList)
	{
		ExecuteWithDrawScaffold(commandList, {});
	}

	void OpenGLRenderCommandExecutor::ExecuteWithDrawScaffold(const RenderCommandList& commandList, const std::function<void()>& drawBlock)
	{
		bool hasDrawCommand = false;

		for (const auto& command : commandList.GetCommands())
		{
			const auto validation = RenderCommandValidator::Validate(command);
			if (!validation.isValid)
				continue;

			std::visit([&](const auto& typed)
			{
				using T = std::decay_t<decltype(typed)>;
				if constexpr (std::is_same_v<T, RenderClearCommand>)
				{
					GLbitfield clearMask = 0;
					if (typed.clearColor)
					{
						glClearColor(typed.color[0], typed.color[1], typed.color[2], typed.color[3]);
						clearMask |= GL_COLOR_BUFFER_BIT;
					}
					if (typed.clearDepth)
					{
						glClearDepth(typed.depth);
						clearMask |= GL_DEPTH_BUFFER_BIT;
					}
					if (typed.clearStencil)
					{
						glClearStencil(static_cast<GLint>(typed.stencil));
						clearMask |= GL_STENCIL_BUFFER_BIT;
					}
					if (clearMask != 0)
						glClear(clearMask);
				}
				else if constexpr (std::is_same_v<T, RenderDrawIndexedCommand>)
				{
					hasDrawCommand = hasDrawCommand || (typed.indexCount > 0 && typed.pipelineHandle != 0 && typed.meshHandle != 0);
				}
				else
				{
					// Scaffold: frame boundary commands are currently metadata-only.
				}
			}, command);
		}

		if (hasDrawCommand && drawBlock)
			drawBlock();
	}
}
