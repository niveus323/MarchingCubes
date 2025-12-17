#include "pch.h"
#include "DXAppBase.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Rendering/Memory/GpuAllocator.h"
#include "Core/Rendering/Memory/StaticBufferRegistry.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Input/InputState.h"
#include "Core/Rendering/RenderSystem.h"
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
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
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
	InitGpuTimeStampResources();
	InitPipeline();
	InitSubsystems();
	OnAfterSwapchainCreated();                      
	InitializeScene();
	InitUI(m_commandList.Get());
}

void DXAppBase::OnDestroy()
{
	if (m_commandQueue && m_swapChainFence)
	{
		const uint64_t finalFence = ++m_nextFenceValue;
		ThrowIfFailed(m_commandQueue->Signal(m_swapChainFence.Get(), finalFence));
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(finalFence, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	if (m_currentScene) m_currentScene->OnExit();
	if (m_uiRenderer)	m_uiRenderer->ShutDown();
	if (m_swapChain)  m_swapChain->SetFullscreenState(FALSE, nullptr);

	DestroyGpuTimeStampResources();
	DestroyFenceAndEvent();
	DestroyBackbuffersAndDefaultDSV();

	m_width = m_height = 0;
}

void DXAppBase::Render()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
	PrepareRender();
	RenderFrame(m_commandList.Get());
	if (m_uiRenderer)
	{
		m_uiRenderer->RenderFrame(m_commandList.Get());
	}
	GpuTimestampEndAndResolve(m_commandList.Get(), m_frameIndex);

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

	if (m_currentScene)
	{
		// 지금은 App 화면 전체를 씬 뷰포트로 사용하므로 x,y를 모두 0으로 세팅
		m_currentScene->OnResize(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
	}

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
	m_inputState->Update();
	OnUpdate(deltaTime);
	OnUpdateUI(deltaTime);

	if (!m_uiRenderer->IsCapturingUI())
	{
		m_currentScene->Update(deltaTime);
	}
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

void DXAppBase::OnPlatformEvent(uint32_t msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
		{
			m_inputState->OnKeyDown(wParam);
		}
		break;
		case WM_KEYUP:
		{
			m_inputState->OnKeyUp(wParam);
		}
		break;
		case WM_MOUSEMOVE:
		{
			m_inputState->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			m_inputState->OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (msg == WM_LBUTTONDOWN) ? VK_LBUTTON : VK_RBUTTON);
		}
		break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		{
			m_inputState->OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (msg == WM_LBUTTONUP) ? VK_LBUTTON : VK_RBUTTON);
		}
		break;
		default:
			break;
	}
}

void DXAppBase::LoadScene(std::unique_ptr<Scene> newScene)
{
	// 기존 씬 해제
	if (m_currentScene)
	{
		m_currentScene->OnExit();
		m_currentScene.reset();
	}

	// 새 씬으로 교체
	m_currentScene = std::move(newScene);
	m_currentScene->OnResize(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
	m_currentScene->Init();
}


void DXAppBase::InitUI(ID3D12GraphicsCommandList* cmd)
{
	if (m_currentScene)
	{
		m_currentScene->InitUI(m_uiRenderer.get());
	}
}

void DXAppBase::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	// 렌더 타겟 생성됨, Back Buffer를 RenderTarget 상태로 전환
	const auto rtvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackbuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmd->ResourceBarrier(1, &rtvBarrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// RTV Clear 명령 추가.
	const float clearColor[] = { 0.0f, 0.0f, 0.2f, 1.0f };
	cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	if (m_currentScene)
	{
		cmd->RSSetViewports(1, &m_currentScene->GetViewport());
		cmd->RSSetScissorRects(1, &m_currentScene->GetScissorRect());
	}
	else
	{
		cmd->RSSetViewports(1, &m_viewport);
		cmd->RSSetScissorRects(1, &m_scissorRect);
	}

	DescriptorAllocator* descriptorAllocator = GetDescriptorAllocator();
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorAllocator->GetCbvSrvUavHeap(), descriptorAllocator->GetSamplerHeap(0) };
	cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	if (ResourceManager* resourceManager = GetResourceManager()) resourceManager->BindDescriptorTable(cmd);

	m_renderSystem->RenderFrame(cmd, GetUploadContext());
}

void DXAppBase::InitSubsystems()
{
	m_gpuAllocator = std::make_unique<GpuAllocator>(m_device.Get());
	m_staticBufferRegistry = std::make_unique<StaticBufferRegistry>(m_device.Get());
	m_descriptorAllocator = std::make_unique<DescriptorAllocator>(m_device.Get());
	m_uploadContext = std::make_unique<UploadContext>(m_device.Get(), m_gpuAllocator.get(), m_staticBufferRegistry.get(), m_descriptorAllocator.get());
	m_resourceManager = std::make_unique<ResourceManager>(m_device.Get(), m_uploadContext.get(), m_descriptorAllocator.get());
	m_renderSystem = std::make_unique<RenderSystem>(m_device.Get(), m_rootSignature.Get(), m_inputElements, GetPSOFiles());
	m_inputState = std::make_unique<InputState>();

	EngineCore::SetDevice(m_device.Get());
	EngineCore::SetRenderSystem(m_renderSystem.get());
	EngineCore::SetDescriptorAllocator(m_descriptorAllocator.get());
	EngineCore::SetUploadContext(m_uploadContext.get());
	EngineCore::SetResourceManager(m_resourceManager.get());
	EngineCore::SetInputState(m_inputState.get());
}

void DXAppBase::OnAfterChainSwaped()
{
	EngineCore::SetFrameIndex(m_frameIndex);
	m_descriptorAllocator->ResetDynamicSlots(m_frameIndex);

	// GPU TimeStamp Readback
	const double gpuMs = ComputeGpuFrameMsAfterCompleted(m_frameIndex);
	// Timer에 GPU 프레임 시간(ms) 반영 → 평균/즉시 FPS 계산에 사용
	GetTimer().PushGpuFrameMs(gpuMs);
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

void DXAppBase::InitPipeline()
{
	CreateRootSignature();
	CreateInputElements();
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

void DXAppBase::InitializeScene()
{
	ThrowIfFailed(m_commandAllocators[0]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));
	OnBuildInitialScene(m_commandList.Get());
	LoadScene(std::move(CreateDefaultScene()));
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
	GpuTimestampBegin(m_commandList.Get(), m_frameIndex);
	OnUpload(m_commandList.Get());
	if (m_currentScene) m_currentScene->Render();
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

void DXAppBase::InitGpuTimeStampResources()
{
	ThrowIfFailed(GetPresentQueue()->GetTimestampFrequency(&m_tsFreq));

	// Query heap: 프레임당 2(시작/끝) * kFrameCount
	D3D12_QUERY_HEAP_DESC qh{};
	qh.Count = kFrameCount * 2;
	qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	qh.NodeMask = 0;
	ThrowIfFailed(m_device->CreateQueryHeap(&qh, IID_PPV_ARGS(&m_tsQueryHeap)));

	// Readback buffer
	const uint64_t bufSize = sizeof(uint64_t) * (static_cast<uint64_t>(kFrameCount * 2));
	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC rb{};
	rb.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rb.Width = bufSize;
	rb.Height = 1;
	rb.DepthOrArraySize = 1;
	rb.MipLevels = 1;
	rb.Format = DXGI_FORMAT_UNKNOWN;
	rb.SampleDesc = { 1,0 };
	rb.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ThrowIfFailed(m_device->CreateCommittedResource(
		&hp, D3D12_HEAP_FLAG_NONE, &rb,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_tsReadback)));

	// map
	ThrowIfFailed(m_tsReadback->Map(0, nullptr, reinterpret_cast<void**>(&m_tsMapped)));
}

void DXAppBase::DestroyGpuTimeStampResources()
{
	if (m_tsReadback)
	{
		m_tsReadback->Unmap(0, nullptr);
		m_tsMapped = nullptr;
	}
	m_tsReadback.Reset();
	m_tsQueryHeap.Reset();
	m_tsFreq = 0;
}

void DXAppBase::GpuTimestampBegin(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex)
{
	const uint32_t q0 = QueryIndexStart(frameIndex);
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0);
}

void DXAppBase::GpuTimestampEndAndResolve(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex)
{
	const uint32_t q0 = QueryIndexStart(frameIndex);
	const uint32_t q1 = q0 + 1;
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q1);

	// 시작/끝 2개를 readback 버퍼에 연속으로 Resolve
	const uint64_t dstOffsetBytes = sizeof(uint64_t) * q0;
	cmd->ResolveQueryData(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0, 2, m_tsReadback.Get(), dstOffsetBytes);
}

double DXAppBase::ComputeGpuFrameMsAfterCompleted(uint32_t frameIndex)
{
	if (!m_tsMapped || m_tsFreq == 0) return 0.0;
	const uint32_t q0 = QueryIndexStart(frameIndex);
	const uint32_t q1 = q0 + 1;
	const uint64_t t0 = m_tsMapped[q0];
	const uint64_t t1 = m_tsMapped[q1];
	if (t1 <= t0) return 0.0;

	const double sec = double(t1 - t0) / double(m_tsFreq);
	return sec * 1000.0;
}