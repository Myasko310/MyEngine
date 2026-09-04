#pragma once

#include "renderer/RenderPlatform.h"

#ifdef _WIN32
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

struct GLFWwindow;

namespace MyEngine
{
	class DirectX12RenderPlatform final : public IRenderPlatform
	{
	public:
		DirectX12RenderPlatform() = default;
		~DirectX12RenderPlatform() override;

		bool Initialize(int width, int height, const char* title) override;
		void Shutdown() override;
		void PollEvents() override;
		void Present() override;
		bool ShouldClose() const override;
		GLFWwindow* GetWindow() const override;

	private:
#ifdef _WIN32
		static constexpr unsigned int kFrameCount = 2;

		bool CreateDeviceResources();
		bool CreateSwapchainResources(int width, int height);
		bool EnsureSwapchainSizeMatchesWindow();
		bool ResizeSwapchain(int width, int height);
		void RebuildRenderTargets();
		bool RebuildDepthStencil(int width, int height);
		bool CreatePipelineResources();
		bool CreateTriangleGeometry();
		bool CreateFrameConstantBuffers();
		void UpdateFrameConstants();
		void RecordAndSubmitClearPass();
		void WaitForGpu();
		void MoveToNextFrame();

		GLFWwindow* m_Window = nullptr;
		bool m_Initialized = false;
		unsigned int m_FrameIndex = 0;
		unsigned int m_RtvDescriptorSize = 0;
		unsigned int m_DsvDescriptorSize = 0;
		int m_BackbufferWidth = 0;
		int m_BackbufferHeight = 0;
		D3D12_VIEWPORT m_Viewport = {};
		D3D12_RECT m_ScissorRect = {};
		float m_ClearColor[4] = { 0.08f, 0.09f, 0.11f, 1.0f };
		float m_ClearDepth = 1.0f;
		unsigned int m_ClearStencil = 0;
		HANDLE m_FenceEvent = nullptr;
		unsigned long long m_FenceValues[kFrameCount] = {};

		Microsoft::WRL::ComPtr<IDXGIFactory6> m_Factory;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> m_Adapter;
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderTargets[kFrameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthStencil;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocators[kFrameCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexUploadBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexUploadBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_FrameConstantBuffers[kFrameCount];
		unsigned char* m_FrameConstantBufferCpuPtrs[kFrameCount] = {};
		unsigned int m_FrameConstantBufferSize = 0;
		double m_AnimationTimeSeconds = 0.0;
		D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};
		unsigned int m_IndexCount = 0;
#else
		GLFWwindow* m_Window = nullptr;
#endif
	};
}
