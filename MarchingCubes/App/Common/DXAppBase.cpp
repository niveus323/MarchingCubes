#include "pch.h"
#include "DXAppBase.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/Memory/GpuAllocator.h"
#include "Core/Rendering/Memory/StaticBufferRegistry.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
using namespace Microsoft::WRL;

static inline bool IsTearingSupported(IDXGIFactory6* factory) {
	BOOL allowTearing = FALSE;
	if (factory) {
		factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	}
	return !!allowTearing;
}

DXAppBase::DXAppBase(uint32_t width, uint32_t height, std::wstring name) : 
	m_width(width),
	m_height(height),
	m_aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
	m_userWarpDevice(false),
	m_title(name)
{
	std::fill(std::begin(m_fenceValues), std::end(m_fenceValues), 0ull);
}

DXAppBase::~DXAppBase() = default;

void DXAppBase::OnInit()
{
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(Win32Application::GetHwnd(), GetPresentQueue());
	CreateBackbuffersAndDefaultDSV(m_width, m_height);  
	CreateFenceAndEvent();
	CreateCommandObjects();
	CreateSubsystems();
	OnAfterSwapchainCreated();                      
	OnInitPipelines();
	InitializeScene();
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

void DXAppBase::Render()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
	PrepareRender();
	OnRender();

	// 렌더링 끝났음. Present 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackbuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	uint32_t syncInterval = 1;
	uint32_t flags = 0;
	if (m_tearingSupported)
	{
		syncInterval = 0;
		flags = DXGI_PRESENT_ALLOW_TEARING;
	}
	ThrowIfFailed(m_swapChain->Present(syncInterval, flags));
	
	MoveToNextFrame();
}

void DXAppBase::OnResize(uint32_t width, uint32_t height)
{
	if (!m_swapChain) return;
	if (width == 0 || height == 0) return;

	m_width = width;
	m_height = height;

	DestroyBackbuffersAndDefaultDSV();
	ThrowIfFailed(m_swapChain->ResizeBuffers( kFrameCount, width, height, m_backbufferFormat, m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	CreateBackbuffersAndDefaultDSV(width, height);
	OnAfterSwapchainCreated(); // 파생: 오프스크린 리소스 재생성 등
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

void DXAppBase::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT_ALIAS(m_commandQueue, L"Main_Graphics_Queue");
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

// CommandAllocator, CommandList 생성, 초기 FenceValue 세팅
void DXAppBase::CreateCommandObjects()
{
	m_nextFenceValue = m_swapChainFence->GetCompletedValue();
	for (uint32_t n = 0; n < kFrameCount; n++)
	{
		m_fenceValues[n] = m_nextFenceValue;
		//CommandAllocator 생성
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocators[n].ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT_INDEXED(m_commandAllocators, n);
	}

	// Create CommandList.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_commandList);
	ThrowIfFailed(m_commandList->Close());
}

void DXAppBase::CreateBackbuffersAndDefaultDSV(uint32_t width, uint32_t height)
{
	//Descriptor Heap 생성
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

	// DSV 생성
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


void DXAppBase::CreateSubsystems()
{
	m_gpuAllocator = std::make_unique<GpuAllocator>(m_device.Get());
	m_staticBufferRegistry = std::make_unique<StaticBufferRegistry>(m_device.Get());
	m_descriptorAllocator = std::make_unique<DescriptorAllocator>(m_device.Get());
	m_uploadContext = std::make_unique<UploadContext>(m_device.Get(), m_gpuAllocator.get(), m_staticBufferRegistry.get(), m_descriptorAllocator.get());
	m_resourceManager = std::make_unique<ResourceManager>(m_device.Get(), m_uploadContext.get(), m_descriptorAllocator.get());
	OnInitSubSystems();
}

void DXAppBase::InitializeScene()
{
	ThrowIfFailed(m_commandAllocators[0]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));
	OnBuildInitialScene(m_commandList.Get());
	if (m_resourceManager) m_resourceManager->BuildTables(m_commandList.Get());
	// Close CommandList
	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();
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

void DXAppBase::PrepareRender()
{
	// 이번 프레임에 할당 가능한 공간 체크
	m_uploadContext->Reclaim(m_swapChainFence->GetCompletedValue());
	m_gpuAllocator->Reclaim(m_swapChainFence->GetCompletedValue());
	OnUpload(m_commandList.Get());
	m_resourceManager->syncGpu(m_commandList.Get());
	m_uploadContext->Execute(m_commandList.Get());
}

void DXAppBase::MoveToNextFrame()
{
	const uint32_t prevIndex = m_frameIndex;
	const uint64_t fenceToSignal = ++m_nextFenceValue;

	if (fenceToSignal > 0) ThrowIfFailed(GetPresentQueue()->Signal(m_swapChainFence.Get(), fenceToSignal));

	m_uploadContext->TrackPendingAllocations(fenceToSignal);
	m_fenceValues[prevIndex] = fenceToSignal;
	// 체인 스왑
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	
	// 체인 스왑 전에 완료하지 못한 작업은 대기
	uint64_t lastFenceValue = m_fenceValues[m_frameIndex];
	if (lastFenceValue > 0 && m_swapChainFence->GetCompletedValue() < lastFenceValue)
	{
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(lastFenceValue, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}
	m_descriptorAllocator->ResetDynamicSlots(m_frameIndex);

	OnAfterChainSwaped();
}

void DXAppBase::WaitForGpu()
{
	assert(m_swapChainFence);
	uint64_t lastFenceValue = m_swapChainFence->GetCompletedValue();
	if (lastFenceValue == 0)
	{
		const uint64_t kInitFence = 1;
		m_nextFenceValue = std::max<uint64_t>(m_nextFenceValue, kInitFence);
		for (uint32_t i = 0; i < kFrameCount; ++i)
		{
			m_fenceValues[i] = std::max<uint64_t>(m_fenceValues[i], kInitFence);
		}
		ThrowIfFailed(GetPresentQueue()->Signal(m_swapChainFence.Get(), kInitFence));
	}
	else if (m_swapChainFence->GetCompletedValue() < lastFenceValue)
	{
		ThrowIfFailed(GetPresentQueue()->Signal(m_swapChainFence.Get(), lastFenceValue));
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(lastFenceValue, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	}
	m_fenceValues[m_frameIndex] = lastFenceValue + 1;
}
