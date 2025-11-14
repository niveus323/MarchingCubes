#include "pch.h"
#include "DXAppBase.h"
using namespace Microsoft::WRL;

static inline bool IsTearingSupported(IDXGIFactory6* factory) {
	BOOL allowTearing = FALSE;
	if (factory) {
		factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	}
	return !!allowTearing;
}

void DXAppBase::OnInit()
{
	CreateDevice();
	CreateQueues();
	CreateSwapChain(Win32Application::GetHwnd(), GetPresentQueue());
	CreateBackbuffersAndDefaultDSV(m_width, m_height);  
	CreateFenceAndEvent();                          
	OnAfterSwapchainCreated();                      
	OnInitPipelines();                              
	OnBuildInitialScene();                          
}

void DXAppBase::OnDestroy()
{
	if (m_swapChain) {
		m_swapChain->SetFullscreenState(FALSE, nullptr);
	}

	DestroyFenceAndEvent();
	DestroyBackbuffersAndDefaultDSV();

	m_width = m_height = 0;
}

void DXAppBase::OnResize(uint32_t width, uint32_t height)
{
	if (!m_swapChain) return;
	if (width == 0 || height == 0) return;

	m_width = width;
	m_height = height;

	DestroyBackbuffersAndDefaultDSV();
	ThrowIfFailed(m_swapChain->ResizeBuffers( kFrameCount, width, height, m_backbufferFormat, m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
	UpdateFrameIndexFromSwapchain();
	CreateBackbuffersAndDefaultDSV(width, height);
	OnAfterSwapchainCreated(); // 颇积: 坷橇胶农赴 府家胶 犁积己 殿
}

void DXAppBase::StartTimer()
{
	m_timer.Start();
}

void DXAppBase::TickAndUpdate()
{
	float deltaTime = m_timer.Tick();
	OnUpdate(deltaTime);
}

_Use_decl_annotations_
void DXAppBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
		{
			m_userWarpDevice = true;
			m_title = m_title + L" (WARP)";
		}
	}
}

std::wstring DXAppBase::GetAssetFullPath(LPCWSTR assetName)
{
	return GetFullPath(AssetType::Default, assetName);
}

_Use_decl_annotations_
void DXAppBase::GetHawrdwardAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHightPerformanceAdapter)
{
	*ppAdapter = nullptr;

	ComPtr<IDXGIAdapter1> adapter;

	ComPtr<IDXGIFactory6> factory6;
	
	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (uint32_t adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, requestHightPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter))); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	if (adapter.Get() == nullptr)
	{
		for (uint32_t adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	*ppAdapter = adapter.Detach();
}

void DXAppBase::SetCustomWindowText(LPCWSTR text) const
{
	std::wstring windowText = m_title + L": " + text;
	SetWindowText(Win32Application::GetHwnd(), windowText.c_str());
}

void DXAppBase::CreateDevice()
{
	uint32_t dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(m_factory.ReleaseAndGetAddressOf())));
	m_tearingSupported = IsTearingSupported(m_factory.Get());

	if (m_userWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf())));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHawrdwardAdapter(m_factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf())));
	}
	NAME_D3D12_OBJECT(m_device);
}

void DXAppBase::CreateSwapChain(HWND hwnd, ID3D12CommandQueue* presentQueue)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = kFrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = m_backbufferFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(m_factory->CreateSwapChainForHwnd(presentQueue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));

	ThrowIfFailed(swapChain.As(&m_swapChain));

	//FullScreen Support Setting
	ThrowIfFailed(m_factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
}

void DXAppBase::CreateBackbuffersAndDefaultDSV(uint32_t width, uint32_t height)
{
	//Descriptor Heap 积己
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.NumDescriptors = kFrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_rtvHeap);

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (uint32_t n = 0; n < kFrameCount; n++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);

		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
	dsvDesc.NumDescriptors = 1;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(m_dsvHeap.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_dsvHeap);
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// DSV 积己
	D3D12_CLEAR_VALUE clear{};
	clear.Format = m_depthFormat;
	clear.DepthStencil.Depth = 1.0f;
	clear.DepthStencil.Stencil = 0;

	CD3DX12_RESOURCE_DESC dsDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_depthFormat, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&dsDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clear,
		IID_PPV_ARGS(m_depthStencil.ReleaseAndGetAddressOf()))
	);
	NAME_D3D12_OBJECT(m_depthStencil);

	m_device->CreateDepthStencilView(m_depthStencil.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void DXAppBase::DestroyBackbuffersAndDefaultDSV()
{
	for (auto& rt : m_renderTargets)
	{
		rt.Reset();
	}
	m_depthStencil.Reset();
	m_rtvHeap.Reset();
	m_dsvHeap.Reset();
}

void DXAppBase::CreateFenceAndEvent()
{
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_swapChainFence.ReleaseAndGetAddressOf())));	
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_fenceEvent) throw std::runtime_error("CreateEvent failed");
}

void DXAppBase::DestroyFenceAndEvent()
{
	m_swapChainFence.Reset();
	if (m_fenceEvent) 
	{ 
		CloseHandle(m_fenceEvent); 
		m_fenceEvent = nullptr; 
	}
}

void DXAppBase::UpdateFrameIndexFromSwapchain()
{
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
