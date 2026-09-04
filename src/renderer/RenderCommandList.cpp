#include "renderer/RenderCommandList.h"

namespace MyEngine
{
	void RenderCommandList::Reset()
	{
		m_Commands.clear();
	}

	void RenderCommandList::BeginFrame()
	{
		m_Commands.emplace_back(RenderBeginFrameCommand{});
	}

	void RenderCommandList::EndFrame()
	{
		m_Commands.emplace_back(RenderEndFrameCommand{});
	}

	void RenderCommandList::Clear(const RenderClearCommand& command)
	{
		m_Commands.emplace_back(command);
	}

	void RenderCommandList::DrawIndexed(const RenderDrawIndexedCommand& command)
	{
		m_Commands.emplace_back(command);
	}

	const std::vector<RenderCommand>& RenderCommandList::GetCommands() const
	{
		return m_Commands;
	}

	bool RenderCommandList::Empty() const
	{
		return m_Commands.empty();
	}
}
