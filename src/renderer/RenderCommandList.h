#pragma once

#include "renderer/RenderCommand.h"

#include <vector>

namespace MyEngine
{
	class RenderCommandList
	{
	public:
		void Reset();
		void BeginFrame();
		void EndFrame();
		void Clear(const RenderClearCommand& command);
		void DrawIndexed(const RenderDrawIndexedCommand& command);

		const std::vector<RenderCommand>& GetCommands() const;
		bool Empty() const;

	private:
		std::vector<RenderCommand> m_Commands;
	};
}
