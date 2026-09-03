#include "renderer/DirectX12RenderCommandExecutor.h"

#include "renderer/RenderCommandValidator.h"

#include <type_traits>
#include <variant>

namespace MyEngine
{
	void DirectX12RenderCommandExecutor::Execute(const RenderCommandList& commandList)
	{
		m_ClearCommandCount = 0;
		m_DrawIndexedCommandCount = 0;

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
					++m_ClearCommandCount;
				}
				else if constexpr (std::is_same_v<T, RenderDrawIndexedCommand>)
				{
					++m_DrawIndexedCommandCount;
				}
			}, command);
		}
	}

#ifdef _WIN32
	void DirectX12RenderCommandExecutor::ExecuteNative(const RenderCommandList& commandList, const DirectX12CommandExecutionContext& context)
	{
		m_ClearCommandCount = 0;
		m_DrawIndexedCommandCount = 0;

		if (!context.commandList)
			return;

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
					if (typed.clearColor)
						context.commandList->ClearRenderTargetView(context.rtvHandle, typed.color, 0, nullptr);

					if (typed.clearDepth || typed.clearStencil)
					{
						D3D12_CLEAR_FLAGS flags = static_cast<D3D12_CLEAR_FLAGS>(0);
						if (typed.clearDepth)
							flags = static_cast<D3D12_CLEAR_FLAGS>(flags | D3D12_CLEAR_FLAG_DEPTH);
						if (typed.clearStencil)
							flags = static_cast<D3D12_CLEAR_FLAGS>(flags | D3D12_CLEAR_FLAG_STENCIL);

						if (flags != 0)
							context.commandList->ClearDepthStencilView(context.dsvHandle, flags, typed.depth, static_cast<UINT8>(typed.stencil), 0, nullptr);
					}

					++m_ClearCommandCount;
				}
				else if constexpr (std::is_same_v<T, RenderDrawIndexedCommand>)
				{
					const bool handlesMatch = (typed.pipelineHandle != 0 && typed.meshHandle != 0 && typed.pipelineHandle == context.activePipelineHandle && typed.meshHandle == context.activeMeshHandle);
					if (context.hasBoundTriangleGeometry && context.pipelineState && context.rootSignature && context.frameConstantBufferAddress != 0 && typed.indexCount > 0 && handlesMatch)
					{
						context.commandList->SetGraphicsRootSignature(context.rootSignature);
						context.commandList->SetGraphicsRootConstantBufferView(0, context.frameConstantBufferAddress);
						context.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
						context.commandList->IASetVertexBuffers(0, 1, &context.vertexBufferView);
						context.commandList->IASetIndexBuffer(&context.indexBufferView);
						context.commandList->DrawIndexedInstanced(typed.indexCount, typed.instanceCount, typed.startIndex, typed.baseVertex, typed.startInstance);
					}

					++m_DrawIndexedCommandCount;
				}
			}, command);
		}
	}
#endif

	unsigned int DirectX12RenderCommandExecutor::GetClearCommandCount() const
	{
		return m_ClearCommandCount;
	}

	unsigned int DirectX12RenderCommandExecutor::GetDrawIndexedCommandCount() const
	{
		return m_DrawIndexedCommandCount;
	}
}
