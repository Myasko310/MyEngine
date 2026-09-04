#include "renderer/DirectX12RenderPlatform.h"

#include "renderer/DirectX12RenderCommandExecutor.h"
#include "renderer/RenderCommandList.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>

#ifdef _WIN32
#include <d3dcompiler.h>
#endif

namespace MyEngine
{
	DirectX12RenderPlatform::~DirectX12RenderPlatform()
	{
		Shutdown();
	}

	bool DirectX12RenderPlatform::Initialize(int width, int height, const char* title)
	{
#ifdef _WIN32
		if (m_Initialized)
			return true;

		if (!glfwInit())
		{
			std::cerr << "Failed to initialize GLFW for DirectX12 backend." << std::endl;
			return false;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (!m_Window)
		{
			std::cerr << "Failed to create GLFW window for DirectX12 backend." << std::endl;
			glfwTerminate();
			return false;
		}

		if (!CreateDeviceResources())
		{
			Shutdown();
			return false;
		}

		if (!CreateSwapchainResources(width, height))
		{
			Shutdown();
			return false;
		}

		if (!CreatePipelineResources())
		{
			Shutdown();
			return false;
		}

		if (!CreateTriangleGeometry())
		{
			Shutdown();
			return false;
		}

		if (!CreateFrameConstantBuffers())
		{
			Shutdown();
			return false;
		}

		m_Initialized = true;
		return true;
#else
		(void)width;
		(void)height;
		(void)title;
		std::cerr << "DirectX12 backend is only available on Windows." << std::endl;
		return false;
#endif
	}

	void DirectX12RenderPlatform::Shutdown()
	{
#ifdef _WIN32
		if (!m_Window && !m_Initialized)
			return;

		if (m_Device && m_CommandQueue && m_Fence)
			WaitForGpu();

		if (m_FenceEvent)
		{
			CloseHandle(m_FenceEvent);
			m_FenceEvent = nullptr;
		}

		for (unsigned int i = 0; i < kFrameCount; ++i)
		{
			m_FrameConstantBufferCpuPtrs[i] = nullptr;
			m_FrameConstantBuffers[i].Reset();
			m_RenderTargets[i].Reset();
			m_CommandAllocators[i].Reset();
		}

		m_VertexUploadBuffer.Reset();
		m_VertexBuffer.Reset();
		m_IndexUploadBuffer.Reset();
		m_IndexBuffer.Reset();
		m_PipelineState.Reset();
		m_RootSignature.Reset();
		m_CommandList.Reset();
		m_DepthStencil.Reset();
		m_DsvHeap.Reset();
		m_RtvHeap.Reset();
		m_SwapChain.Reset();
		m_CommandQueue.Reset();
		m_Fence.Reset();
		m_Device.Reset();
		m_Adapter.Reset();
		m_Factory.Reset();

		if (m_Window)
		{
			glfwDestroyWindow(m_Window);
			m_Window = nullptr;
		}

		glfwTerminate();
		m_Initialized = false;
#endif
	}

	void DirectX12RenderPlatform::PollEvents()
	{
		glfwPollEvents();
#ifdef _WIN32
		EnsureSwapchainSizeMatchesWindow();
#endif
	}

	void DirectX12RenderPlatform::Present()
	{
#ifdef _WIN32
		if (!m_SwapChain)
			return;

		if (!EnsureSwapchainSizeMatchesWindow())
			return;

		RecordAndSubmitClearPass();

		HRESULT hr = m_SwapChain->Present(1, 0);
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Present failed: 0x" << std::hex << static_cast<unsigned int>(hr) << std::dec << std::endl;
			return;
		}

		MoveToNextFrame();
#endif
	}

	bool DirectX12RenderPlatform::ShouldClose() const
	{
		return m_Window ? glfwWindowShouldClose(m_Window) != 0 : true;
	}

	GLFWwindow* DirectX12RenderPlatform::GetWindow() const
	{
		return m_Window;
	}

#ifdef _WIN32
	bool DirectX12RenderPlatform::CreateDeviceResources()
	{
		UINT factoryFlags = 0;
		if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory))))
		{
			std::cerr << "[DirectX12] CreateDXGIFactory2 failed." << std::endl;
			return false;
		}

		for (UINT adapterIndex = 0; ; ++adapterIndex)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (m_Factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
				break;

			DXGI_ADAPTER_DESC1 desc = {};
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device))))
			{
				m_Adapter = adapter;
				break;
			}
		}

		if (!m_Device)
		{
			std::cerr << "[DirectX12] Failed to create D3D12 hardware device." << std::endl;
			return false;
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue))))
		{
			std::cerr << "[DirectX12] CreateCommandQueue failed." << std::endl;
			return false;
		}

		for (unsigned int i = 0; i < kFrameCount; ++i)
		{
			if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocators[i]))))
			{
				std::cerr << "[DirectX12] CreateCommandAllocator failed." << std::endl;
				return false;
			}
		}

		if (FAILED(m_Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_CommandAllocators[0].Get(),
			nullptr,
			IID_PPV_ARGS(&m_CommandList))))
		{
			std::cerr << "[DirectX12] CreateCommandList failed." << std::endl;
			return false;
		}

		m_CommandList->Close();

		if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))))
		{
			std::cerr << "[DirectX12] CreateFence failed." << std::endl;
			return false;
		}

		m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_FenceEvent)
		{
			std::cerr << "[DirectX12] CreateEvent failed for fence." << std::endl;
			return false;
		}

		for (unsigned int i = 0; i < kFrameCount; ++i)
			m_FenceValues[i] = 1;

		return true;
	}

	bool DirectX12RenderPlatform::CreatePipelineResources()
	{
		if (!m_Device)
			return false;

		D3D12_ROOT_PARAMETER rootParameter = {};
		rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameter.Descriptor.ShaderRegister = 0;
		rootParameter.Descriptor.RegisterSpace = 0;
		rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.NumParameters = 1;
		rootSigDesc.pParameters = &rootParameter;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureError;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&rootSignatureBlob,
			&rootSignatureError);
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Failed to serialize root signature." << std::endl;
			return false;
		}

		hr = m_Device->CreateRootSignature(
			0,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_RootSignature));
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Failed to create root signature." << std::endl;
			return false;
		}

		static const char* vsSource =
			"cbuffer FrameConstants : register(b0) { row_major float4x4 uMVP; };"
			"struct VSInput { float3 pos : POSITION; float3 col : COLOR; };"
			"struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };"
			"PSInput main(VSInput i){ PSInput o; o.pos=mul(float4(i.pos,1.0), uMVP); o.col=i.col; return o; }";
		static const char* psSource =
			"struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };"
			"float4 main(PSInput i) : SV_TARGET { return float4(i.col, 1.0); }";

		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> shaderErrorBlob;

		hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &shaderErrorBlob);
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Failed to compile vertex shader." << std::endl;
			return false;
		}

		hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &shaderErrorBlob);
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Failed to compile pixel shader." << std::endl;
			return false;
		}

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = TRUE;
		rasterizerDesc.DepthClipEnable = TRUE;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depthStencilDesc.StencilEnable = FALSE;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
		psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
		psoDesc.BlendState = blendDesc;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.DepthStencilState = depthStencilDesc;
		psoDesc.InputLayout = { inputLayout, static_cast<UINT>(_countof(inputLayout)) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;

		hr = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Failed to create graphics pipeline state." << std::endl;
			return false;
		}

		return true;
	}

	bool DirectX12RenderPlatform::CreateTriangleGeometry()
	{
		if (!m_Device || !m_CommandList || !m_CommandQueue)
			return false;

		struct Vertex
		{
			float position[3];
			float color[3];
		};

		const Vertex vertices[] =
		{
			{ { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.2f, 0.2f } },
			{ { 0.25f, -0.25f, 0.0f }, { 0.2f, 1.0f, 0.2f } },
			{ { -0.25f, -0.25f, 0.0f }, { 0.2f, 0.4f, 1.0f } }
		};
		const std::uint16_t indices[] = { 0, 1, 2 };
		const UINT vertexBufferSize = static_cast<UINT>(sizeof(vertices));
		const UINT indexBufferSize = static_cast<UINT>(sizeof(indices));
		m_IndexCount = static_cast<unsigned int>(_countof(indices));

		auto createDefaultAndUpload = [this](UINT size, ID3D12Resource** outDefault, ID3D12Resource** outUpload) -> bool
		{
			D3D12_HEAP_PROPERTIES defaultHeap = {};
			defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
			D3D12_HEAP_PROPERTIES uploadHeap = {};
			uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Width = size;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			if (FAILED(m_Device->CreateCommittedResource(
				&defaultHeap,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(outDefault))))
			{
				return false;
			}

			if (FAILED(m_Device->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(outUpload))))
			{
				return false;
			}

			return true;
		};

		if (!createDefaultAndUpload(vertexBufferSize, m_VertexBuffer.GetAddressOf(), m_VertexUploadBuffer.GetAddressOf()))
			return false;
		if (!createDefaultAndUpload(indexBufferSize, m_IndexBuffer.GetAddressOf(), m_IndexUploadBuffer.GetAddressOf()))
			return false;

		void* mapped = nullptr;
		D3D12_RANGE range = { 0, 0 };
		if (FAILED(m_VertexUploadBuffer->Map(0, &range, &mapped)))
			return false;
		std::memcpy(mapped, vertices, vertexBufferSize);
		m_VertexUploadBuffer->Unmap(0, nullptr);

		if (FAILED(m_IndexUploadBuffer->Map(0, &range, &mapped)))
			return false;
		std::memcpy(mapped, indices, indexBufferSize);
		m_IndexUploadBuffer->Unmap(0, nullptr);

		if (FAILED(m_CommandAllocators[0]->Reset()))
			return false;
		if (FAILED(m_CommandList->Reset(m_CommandAllocators[0].Get(), nullptr)))
			return false;

		m_CommandList->CopyBufferRegion(m_VertexBuffer.Get(), 0, m_VertexUploadBuffer.Get(), 0, vertexBufferSize);
		m_CommandList->CopyBufferRegion(m_IndexBuffer.Get(), 0, m_IndexUploadBuffer.Get(), 0, indexBufferSize);

		D3D12_RESOURCE_BARRIER barriers[2] = {};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = m_VertexBuffer.Get();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource = m_IndexBuffer.Get();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_CommandList->ResourceBarrier(2, barriers);

		if (FAILED(m_CommandList->Close()))
			return false;

		ID3D12CommandList* lists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(1, lists);
		WaitForGpu();

		m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
		m_VertexBufferView.SizeInBytes = vertexBufferSize;
		m_VertexBufferView.StrideInBytes = sizeof(Vertex);

		m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
		m_IndexBufferView.SizeInBytes = indexBufferSize;
		m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;

		return true;
	}

	bool DirectX12RenderPlatform::CreateFrameConstantBuffers()
	{
		if (!m_Device)
			return false;

		struct alignas(16) FrameConstants
		{
			float mvp[16];
		};

		m_FrameConstantBufferSize = (static_cast<unsigned int>(sizeof(FrameConstants)) + 255u) & ~255u;
		D3D12_HEAP_PROPERTIES uploadHeap = {};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = m_FrameConstantBufferSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_RANGE readRange = { 0, 0 };
		for (unsigned int i = 0; i < kFrameCount; ++i)
		{
			if (FAILED(m_Device->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_FrameConstantBuffers[i]))))
			{
				return false;
			}

			void* mapped = nullptr;
			if (FAILED(m_FrameConstantBuffers[i]->Map(0, &readRange, &mapped)))
				return false;
			m_FrameConstantBufferCpuPtrs[i] = static_cast<unsigned char*>(mapped);
		}

		return true;
	}

	bool DirectX12RenderPlatform::CreateSwapchainResources(int width, int height)
	{
		HWND hwnd = glfwGetWin32Window(m_Window);
		if (!hwnd)
		{
			std::cerr << "[DirectX12] Failed to resolve Win32 window handle from GLFW." << std::endl;
			return false;
		}

		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = static_cast<UINT>(width);
		swapchainDesc.Height = static_cast<UINT>(height);
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = kFrameCount;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_Factory->CreateSwapChainForHwnd(
			m_CommandQueue.Get(),
			hwnd,
			&swapchainDesc,
			nullptr,
			nullptr,
			&swapchain)))
		{
			std::cerr << "[DirectX12] CreateSwapChainForHwnd failed." << std::endl;
			return false;
		}

		if (FAILED(swapchain.As(&m_SwapChain)))
		{
			std::cerr << "[DirectX12] Failed to query IDXGISwapChain3." << std::endl;
			return false;
		}

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap))))
		{
			std::cerr << "[DirectX12] CreateDescriptorHeap failed." << std::endl;
			return false;
		}

		m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DsvHeap))))
		{
			std::cerr << "[DirectX12] Create DSV descriptor heap failed." << std::endl;
			return false;
		}
		m_DsvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		RebuildRenderTargets();
		if (!RebuildDepthStencil(width, height))
			return false;

		m_BackbufferWidth = width;
		m_BackbufferHeight = height;
		m_Viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, width, height };
		return true;
	}

	bool DirectX12RenderPlatform::EnsureSwapchainSizeMatchesWindow()
	{
		if (!m_Window || !m_SwapChain)
			return false;

		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		if (width <= 0 || height <= 0)
			return true;

		if (width == m_BackbufferWidth && height == m_BackbufferHeight)
			return true;

		return ResizeSwapchain(width, height);
	}

	bool DirectX12RenderPlatform::ResizeSwapchain(int width, int height)
	{
		if (!m_SwapChain || width <= 0 || height <= 0)
			return false;

		WaitForGpu();
		for (unsigned int i = 0; i < kFrameCount; ++i)
			m_RenderTargets[i].Reset();
		m_DepthStencil.Reset();

		HRESULT hr = m_SwapChain->ResizeBuffers(
			kFrameCount,
			static_cast<UINT>(width),
			static_cast<UINT>(height),
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0);
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] ResizeBuffers failed: 0x" << std::hex << static_cast<unsigned int>(hr) << std::dec << std::endl;
			return false;
		}

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
		m_BackbufferWidth = width;
		m_BackbufferHeight = height;
		m_Viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, width, height };
		RebuildRenderTargets();
		if (!RebuildDepthStencil(width, height))
			return false;
		return true;
	}

	void DirectX12RenderPlatform::RebuildRenderTargets()
	{
		if (!m_SwapChain || !m_RtvHeap || !m_Device)
			return;

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (unsigned int i = 0; i < kFrameCount; ++i)
		{
			m_RenderTargets[i].Reset();
			if (FAILED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]))))
			{
				std::cerr << "[DirectX12] Failed to rebuild swapchain buffer " << i << "." << std::endl;
				continue;
			}

			m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, handle);
			handle.ptr += static_cast<SIZE_T>(m_RtvDescriptorSize);
		}
	}

	bool DirectX12RenderPlatform::RebuildDepthStencil(int width, int height)
	{
		if (!m_Device || !m_DsvHeap || width <= 0 || height <= 0)
			return false;

		m_DepthStencil.Reset();

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC depthDesc = {};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = static_cast<UINT64>(width);
		depthDesc.Height = static_cast<UINT>(height);
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthClearValue.DepthStencil.Depth = m_ClearDepth;
		depthClearValue.DepthStencil.Stencil = static_cast<UINT8>(m_ClearStencil);

		HRESULT hr = m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthClearValue,
			IID_PPV_ARGS(&m_DepthStencil));
		if (FAILED(hr))
		{
			std::cerr << "[DirectX12] Create depth-stencil resource failed: 0x" << std::hex << static_cast<unsigned int>(hr) << std::dec << std::endl;
			return false;
		}

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		m_Device->CreateDepthStencilView(m_DepthStencil.Get(), &dsvDesc, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	void DirectX12RenderPlatform::UpdateFrameConstants()
	{
		if (m_FrameConstantBufferSize == 0 || !m_FrameConstantBufferCpuPtrs[m_FrameIndex] || !m_FrameConstantBuffers[m_FrameIndex])
			return;

		m_AnimationTimeSeconds = glfwGetTime();
		const float angle = static_cast<float>(m_AnimationTimeSeconds);
		const float c = std::cos(angle);
		const float s = std::sin(angle);

		const float aspect = (m_BackbufferHeight > 0) ? static_cast<float>(m_BackbufferWidth) / static_cast<float>(m_BackbufferHeight) : 1.0f;
		const float scaleX = (aspect > 0.0f) ? (1.0f / aspect) : 1.0f;

		const float mvp[16] =
		{
			 c * scaleX,  s, 0.0f, 0.0f,
			-s * scaleX,  c, 0.0f, 0.0f,
			 0.0f,      0.0f, 1.0f, 0.0f,
			 0.0f,      0.0f, 0.0f, 1.0f
		};

		std::memcpy(m_FrameConstantBufferCpuPtrs[m_FrameIndex], mvp, sizeof(mvp));
	}

	void DirectX12RenderPlatform::RecordAndSubmitClearPass()
	{
		if (!m_CommandList || !m_CommandQueue || !m_RtvHeap || !m_DsvHeap || !m_RenderTargets[m_FrameIndex] || !m_DepthStencil)
			return;

		if (FAILED(m_CommandAllocators[m_FrameIndex]->Reset()))
			return;

		if (FAILED(m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), m_PipelineState.Get())))
			return;

		m_CommandList->RSSetViewports(1, &m_Viewport);
		m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

		D3D12_RESOURCE_BARRIER toRenderTarget = {};
		toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		toRenderTarget.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
		toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_CommandList->ResourceBarrier(1, &toRenderTarget);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += static_cast<SIZE_T>(m_FrameIndex) * static_cast<SIZE_T>(m_RtvDescriptorSize);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
		m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		RenderCommandList renderCommands;
		renderCommands.BeginFrame();
		RenderClearCommand clearCommand;
		clearCommand.color[0] = m_ClearColor[0];
		clearCommand.color[1] = m_ClearColor[1];
		clearCommand.color[2] = m_ClearColor[2];
		clearCommand.color[3] = m_ClearColor[3];
		clearCommand.depth = m_ClearDepth;
		clearCommand.stencil = m_ClearStencil;
		clearCommand.clearColor = true;
		clearCommand.clearDepth = true;
		clearCommand.clearStencil = true;
		renderCommands.Clear(clearCommand);

		const bool canDrawTriangle = (m_PipelineState && m_RootSignature && m_VertexBuffer && m_IndexBuffer && m_FrameConstantBuffers[m_FrameIndex] && m_IndexCount > 0);
		if (canDrawTriangle)
		{
			RenderDrawIndexedCommand drawCommand;
			drawCommand.pipelineHandle = 1;
			drawCommand.meshHandle = 1;
			drawCommand.indexCount = m_IndexCount;
			drawCommand.instanceCount = 1;
			renderCommands.DrawIndexed(drawCommand);
		}
		renderCommands.EndFrame();

		UpdateFrameConstants();

		DirectX12CommandExecutionContext commandContext;
		commandContext.commandList = m_CommandList.Get();
		commandContext.rtvHandle = rtvHandle;
		commandContext.dsvHandle = dsvHandle;
		commandContext.rootSignature = m_RootSignature.Get();
		commandContext.pipelineState = m_PipelineState.Get();
		commandContext.vertexBufferView = m_VertexBufferView;
		commandContext.indexBufferView = m_IndexBufferView;
		commandContext.frameConstantBufferAddress = m_FrameConstantBuffers[m_FrameIndex] ? m_FrameConstantBuffers[m_FrameIndex]->GetGPUVirtualAddress() : 0;
		commandContext.hasBoundTriangleGeometry = canDrawTriangle;
		commandContext.activePipelineHandle = 1;
		commandContext.activeMeshHandle = 1;

		static DirectX12RenderCommandExecutor dx12CommandExecutor;
		dx12CommandExecutor.ExecuteNative(renderCommands, commandContext);

		D3D12_RESOURCE_BARRIER toPresent = {};
		toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		toPresent.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
		toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_CommandList->ResourceBarrier(1, &toPresent);

		if (FAILED(m_CommandList->Close()))
			return;

		ID3D12CommandList* lists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(1, lists);
	}

	void DirectX12RenderPlatform::WaitForGpu()
	{
		const unsigned int frame = m_FrameIndex;
		const unsigned long long fenceValue = m_FenceValues[frame];
		if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
			return;

		if (m_Fence->GetCompletedValue() < fenceValue)
		{
			if (FAILED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
				return;
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		m_FenceValues[frame]++;
	}

	void DirectX12RenderPlatform::MoveToNextFrame()
	{
		const unsigned int currentFrame = m_FrameIndex;
		const unsigned long long signalValue = m_FenceValues[currentFrame];
		if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), signalValue)))
			return;

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
		if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
		{
			if (FAILED(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent)))
				return;
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		m_FenceValues[currentFrame] = signalValue + 1;
	}
#endif
}
