#pragma once

#include "renderer/RenderCommandExecutor.h"

#include <cstdint>

#ifdef _WIN32
#include <d3d12.h>
#endif

namespace MyEngine
{
#ifdef _WIN32
	struct DirectX12CommandExecutionContext
	{
		ID3D12GraphicsCommandList* commandList = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pipelineState = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		D3D12_GPU_VIRTUAL_ADDRESS frameConstantBufferAddress = 0;
		bool hasBoundTriangleGeometry = false;
		std::uint64_t activePipelineHandle = 0;
		std::uint64_t activeMeshHandle = 0;
	};
#endif

	class DirectX12RenderCommandExecutor final : public IRenderCommandExecutor
	{
	public:
		void Execute(const RenderCommandList& commandList) override;
#ifdef _WIN32
		void ExecuteNative(const RenderCommandList& commandList, const DirectX12CommandExecutionContext& context);
#endif

		unsigned int GetClearCommandCount() const;
		unsigned int GetDrawIndexedCommandCount() const;

	private:
		unsigned int m_ClearCommandCount = 0;
		unsigned int m_DrawIndexedCommandCount = 0;
	};
}
