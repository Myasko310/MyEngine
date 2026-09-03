#pragma once

#include "renderer/RenderCommandExecutor.h"

#include <functional>

namespace MyEngine
{
	class OpenGLRenderCommandExecutor final : public IRenderCommandExecutor
	{
	public:
		void Execute(const RenderCommandList& commandList) override;
		void ExecuteWithDrawScaffold(const RenderCommandList& commandList, const std::function<void()>& drawBlock);
	};
}
