#include "pch.h"
#include "EditorApp.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Rendering/PSO/PSOSpec.h"
#include <numeric>

void EditorApp::OnDestroy()
{
	if (m_commandQueue && m_swapChainFence)
	{
		const UINT64 completed = m_swapChainFence->GetCompletedValue();
		UINT64 maxFenceToWait = 0;
		for (auto it = m_pendingDeletes.begin(); it != m_pendingDeletes.end(); )
		{
			if (completed >= it->fenceValue)
			{
				// 이미 완료된 항목은 제거
				it = m_pendingDeletes.erase(it);
			}
			else
			{
				// 아직 완료되지 않은 항목 중 최대 펜스값 갱신
				maxFenceToWait = std::max(maxFenceToWait, it->fenceValue);
				++it;
			}
		}

		if (maxFenceToWait == 0)
		{
			// 현재 제출된 프레임의 완료를 보장
			WaitForGpu();
		}
		else
		{
			// 이미 완료된지 재확인 (race-safe)
			const UINT64 now2 = m_swapChainFence->GetCompletedValue();
			if (now2 < maxFenceToWait)
			{
				ThrowIfFailed(m_commandQueue->Signal(m_swapChainFence.Get(), maxFenceToWait));
				ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(maxFenceToWait, m_fenceEvent));
				WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			}
		}

		m_pendingDeletes.erase(
			std::remove_if(m_pendingDeletes.begin(), m_pendingDeletes.end(),
				[&](const PendingDeleteItem& pd) { return completed >= pd.fenceValue; }),
			m_pendingDeletes.end()
		);

		m_pendingDeletes.clear();
		m_toDeletesContainer.clear();
	}

	for (auto& d : m_StaticObjects)
	{
		if (!d) continue;
		GeometryBuffer* buf = d->GetGPUBuffer();
		if (buf) buf->ReleaseGPUResources();
	}

	if (m_commandQueue && m_swapChainFence)
	{
		const UINT64 finalFence = ++m_nextFenceValue;
		ThrowIfFailed(m_commandQueue->Signal(m_swapChainFence.Get(), finalFence));
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(finalFence, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	if (m_uiRenderer)	m_uiRenderer->ShutDown();
	DestroyGpuTimeStampResources();
	DXAppBase::OnDestroy();
}

void EditorApp::OnUpdate(float deltaTime)
{
	UpdateScene(deltaTime);
	UpdateUI(deltaTime);

	m_mainCamera->UpdateViewMatrix();
	m_mainCamera->UpdateProjMatrix();
	m_inputState.Update();

}

void EditorApp::OnRender()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
	GpuTimestampBegin(m_commandList.Get(), m_frameIndex);
	BeforeDraw(m_commandList.Get());
	DrawScene(m_commandList.Get());
	//DrawUI(m_commandList.Get());
	m_uiRenderer->RenderFrame(m_commandList.Get());
	AfterDraw(m_commandList.Get());

	// 렌더링 끝났음. Present 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackbuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	GpuTimestampEndAndResolve(m_commandList.Get(), m_frameIndex);

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	UINT syncInterval = 1;
	UINT flags = 0;
	if (m_tearingSupported)
	{
		syncInterval = 0;
		flags = DXGI_PRESENT_ALLOW_TEARING;
	}
	ThrowIfFailed(m_swapChain->Present(syncInterval, flags));
	MoveToNextFrame();
}

void EditorApp::OnPlatformEvent(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
		{
			m_inputState.OnKeyDown(wParam);
		}
		break;
		case WM_KEYUP:
		{
			m_inputState.OnKeyUp(wParam);
		}
		break;
		case WM_MOUSEMOVE:
		{
			m_inputState.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			m_inputState.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (msg == WM_LBUTTONDOWN) ? VK_LBUTTON : VK_RBUTTON);
		}
		break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		{
			m_inputState.OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (msg == WM_LBUTTONUP) ? VK_LBUTTON : VK_RBUTTON);
		}
		break;
		default:
			break;
	}

}

void EditorApp::CreateQueues()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT_ALIAS(m_commandQueue, L"Main_Graphics_Queue");
}

void EditorApp::OnAfterSwapchainCreated()
{
	m_nextFenceValue = m_swapChainFence->GetCompletedValue();
	for (UINT n = 0; n < kFrameCount; n++)
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

	// SRV Heap 생성
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.ReleaseAndGetAddressOf())));
	NAME_D3D12_OBJECT(m_srvHeap);

	// SRV (Env Texture) 생성
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		m_device->CreateShaderResourceView(nullptr, &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	}
	InitGpuTimeStampResources();
}

void EditorApp::OnInitPipelines()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//Create Main Root Signature.
	{
		// Define Root Parameter : b0 (CameraBuffer), b1 (ObjectBuffer), b2 (MaterialBuffer), b3 (LightBuffer), t0 (EnvMap), s0 (EnvSampler)
		CD3DX12_ROOT_PARAMETER1  rootParams[5];
		ZeroMemory(rootParams, sizeof(rootParams));
		rootParams[0].InitAsConstantBufferView(0); // b0
		rootParams[1].InitAsConstantBufferView(1); // b1
		rootParams[2].InitAsConstantBufferView(2); // b2
		rootParams[3].InitAsConstantBufferView(3); // b3

		// 환경 맵 텍스쳐 슬롯 등록. (샘플러는 Static Sampler 사용)
		CD3DX12_DESCRIPTOR_RANGE1 range{};
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
		rootParams[4].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		// Static Sampler 등록 ( 별도의 샘플러가 필요할 경우 Descriptor Table에 포함할 것.)
		D3D12_STATIC_SAMPLER_DESC samplerDesc{};
		samplerDesc.ShaderRegister = 0; // s0
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, nullptr));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT(m_rootSignature);
	}

	D3D12_INPUT_LAYOUT_DESC inputLayout = { inputElementDesc, _countof(inputElementDesc) };
	m_renderSystem = std::make_unique<RenderSystem>(m_device.Get(), m_rootSignature.Get(), inputLayout, GetPSOFiles());
	m_renderSystem->SetPsoEnabled("Wire", false);
	m_renderSystem->SetPsoEnabled("DrawNormal", false);
}

ID3D12GraphicsCommandList* EditorApp::BeginInitCommand()
{
	ThrowIfFailed(m_commandAllocators[0]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));
	return m_commandList.Get();
}

void EditorApp::OnBuildInitialScene()
{
	ID3D12GraphicsCommandList* initCommand = BeginInitCommand();
	m_mainCamera = std::make_unique<Camera>(static_cast<float>(m_width), static_cast<float>(m_height));
	m_lightManager = std::make_unique<LightManager>(m_device.Get(), 3);

	m_defaultMat = std::make_shared<Material>();
	m_defaultMat->SetAlbedo({ 1.0f, 1.0f, 1.0f });
	m_defaultMat->SetMetallic(0.0f);
	m_defaultMat->SetRoughness(0.5f);
	m_defaultMat->SetSpecularStrength(0.04f);
	m_defaultMat->SetAmbientOcclusion(1.0f);
	m_defaultMat->SetIOR(1.5f);
	m_defaultMat->SetShadingModel(EShadingModel::DefaultLit);
	m_defaultMat->SetOpacity(1.0f);
	m_defaultMat->CreateConstantBuffer(m_device.Get());

	InitScene(initCommand);
	InitUICommon(initCommand);
	EndInitCommand();
}

void EditorApp::EndInitCommand()
{
	// Close CommandList
	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();
}

void EditorApp::InitUICommon(ID3D12GraphicsCommandList* cmd)
{
	std::unique_ptr<ImGUIRenderer> imguiRenderer = std::make_unique<ImGUIRenderer>();

	UI::InitContext initContext = {};
	initContext.device = m_device.Get();

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> srvHeap;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));
	NAME_D3D12_OBJECT(srvHeap);

	ImGUIInitOptions initOptions = {};
	initOptions.commandQueue = GetPresentQueue();
	initOptions.nums_of_frame = kFrameCount;
	initOptions.srvHeap = srvHeap.Get();
	initOptions.cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
	initOptions.gpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
	initOptions.configFlags |= ImGuiConfigFlags_DockingEnable;
	initOptions.configFlags |= ImGuiConfigFlags_ViewportsEnable;
	initContext.userData = std::move(std::any(initOptions));

	if (!imguiRenderer->Initialize(initContext))
	{
		MessageBox(Win32Application::GetHwnd(), m_uiRenderer->GetLastErrorMsg().c_str(), L"UI Initialization Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(-1);
		return;
	}

	m_uiRenderer = std::move(imguiRenderer);
	InitUI();

	// profiler
	m_profilerOwner = std::make_shared<Profiler>();
	m_profiler = m_profilerOwner;
}

void EditorApp::InitUI()
{
	m_uiToken_Fps = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderFpsUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Fps"
		});

	m_uiToken_Profiler = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderProfilingUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Profiler"
		});
}

void EditorApp::UpdateUI(float deltaTime)
{
	if (auto p = m_profiler.lock())
	{
		std::vector<BufferPoolInfo> pools;
		// GpuAllocator
		for (auto& dbg : m_renderSystem->GetGpuAllocator()->GetDebugPools())
		{
			BufferPoolInfo pi;
			pi.name = dbg.name;
			pi.capacity = dbg.pool->GetCapacity();
			std::vector<BufferBlock>& allocated = dbg.pool->GetAllocatedBlocks();
			pi.used = std::accumulate(allocated.begin(), allocated.end(), 0ULL, [](UINT64 sum, const BufferBlock& b) { return sum + b.size; });
			pi.free = dbg.pool->GetFreeBlocks();
			pi.allocated = dbg.pool->GetAllocatedBlocks();
			pools.push_back(pi);
		}

		// StaticBufferRegistry
		if(auto* registry = m_renderSystem->GetStaticBufferRegistry())
		{
			BufferPoolInfo pi_vb;
			pi_vb.name = "StaticVB";
			pi_vb.capacity = registry->GetVBCapacity();
			std::vector<BufferBlock>& allocatedVB = registry->GetVBAllocated();
			pi_vb.used = std::accumulate(allocatedVB.begin(), allocatedVB.end(), 0ULL, [](UINT64 sum, const BufferBlock& b) {return sum + b.size; });
			pi_vb.free = registry->GetVBFree();
			pi_vb.allocated = allocatedVB;
			pools.push_back(pi_vb);

			BufferPoolInfo pi_ib;
			pi_ib.name = "StaticIB";
			pi_ib.capacity = registry->GetIBCapacity();
			std::vector<BufferBlock>& allocatedIB = registry->GetIBAllocated();
			pi_ib.used = std::accumulate(allocatedIB.begin(), allocatedIB.end(), 0ULL, [](UINT64 sum, const BufferBlock& b) {return sum + b.size; });
			pi_ib.free = registry->GetIBFree();
			pi_ib.allocated = allocatedIB;
			pools.push_back(pi_ib);
		}
		p->SetBufferPools(pools);

		p->UpdateFrame(GetTimer().GetTimeMs());
	}
}

void EditorApp::BeforeDraw(ID3D12GraphicsCommandList* cmd)
{
	// 교체되는 리소스를 Sink에 수집.
	m_toDeletesContainer.clear();
	m_renderSystem->PrepareRender(cmd);

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

	cmd->RSSetViewports(1, &m_viewport);
	cmd->RSSetScissorRects(1, &m_scissorRect);
}

void EditorApp::DrawScene(ID3D12GraphicsCommandList* cmd)
{
	// Set necessary state.
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());

	// Descriptor Heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(1, ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(4, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

	CameraConstants commonCameraData = m_mainCamera->BuildCameraConstants();
	LightBlobView commonLightData = m_lightManager->BuildLightConstants();

	m_renderSystem->RenderFrame(cmd, commonCameraData, commonLightData);
}

void EditorApp::MoveToNextFrame()
{
	const UINT prevIndex = m_frameIndex;
	const UINT64 fenceToSignal = ++m_nextFenceValue;

	if (fenceToSignal > 0) ThrowIfFailed(GetPresentQueue()->Signal(m_swapChainFence.Get(), fenceToSignal));

	PendingDeleteItem pd = m_renderSystem->CleanUp(fenceToSignal);
	if (!pd.resources.empty())
	{
		if (pd.fenceValue == 0)
		{
			Log::Print("EditorApp", "Warning: CleanUp returned pending delete with zero fenceValue!");
			pd.fenceValue = fenceToSignal;
		}
		m_pendingDeletes.emplace_back(std::move(pd));
	}

	if (!m_toDeletesContainer.empty())
	{
		PendingDeleteItem pd;
		pd.fenceValue = fenceToSignal;
		pd.resources.swap(m_toDeletesContainer);
		m_pendingDeletes.emplace_back(std::move(pd));
	}

	m_fenceValues[prevIndex] = fenceToSignal;
	// 체인 스왑
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// 체인 스왑 전에 완료하지 못한 작업은 대기
	UINT64 lastFenceValue = m_fenceValues[m_frameIndex];
	if (lastFenceValue > 0 && m_swapChainFence->GetCompletedValue() < lastFenceValue)
	{
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(lastFenceValue, m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// GPU TimeStamp Readback
	const double gpuMs = ComputeGpuFrameMsAfterCompleted(m_frameIndex);
	// Timer에 GPU 프레임 시간(ms) 반영 → 평균/즉시 FPS 계산에 사용
	GetTimer().PushGpuFrameMs(gpuMs);

	// 삭제 요청 이행.
	const UINT64 completed = m_swapChainFence->GetCompletedValue();
	auto iter = m_pendingDeletes.begin();
	while (iter != m_pendingDeletes.end())
	{
		if (completed >= iter->fenceValue)
		{
			iter = m_pendingDeletes.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	// RenderSystem 펜스 값 갱신
	m_renderSystem->CallWhenFenceSignaled(completed);
}

void EditorApp::WaitForGpu()
{
#ifdef _DEBUG
	assert(m_swapChainFence);
#endif // _DEBUG

	UINT64 lastFenceValue = m_swapChainFence->GetCompletedValue();
	if (lastFenceValue == 0)
	{
		const UINT64 kInitFence = 1;
		m_nextFenceValue = std::max<UINT64>(m_nextFenceValue, kInitFence);
		for (UINT i = 0; i < kFrameCount; ++i)
		{
			m_fenceValues[i] = std::max<UINT64>(m_fenceValues[i], kInitFence);
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

void EditorApp::InitGpuTimeStampResources()
{
	ThrowIfFailed(GetPresentQueue()->GetTimestampFrequency(&m_tsFreq));

	// Query heap: 프레임당 2(시작/끝) * kFrameCount
	D3D12_QUERY_HEAP_DESC qh{};
	qh.Count = kFrameCount * 2;
	qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	qh.NodeMask = 0;
	ThrowIfFailed(m_device->CreateQueryHeap(&qh, IID_PPV_ARGS(&m_tsQueryHeap)));

	// Readback buffer
	const UINT64 bufSize = sizeof(UINT64) * (kFrameCount * 2);
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

void EditorApp::DestroyGpuTimeStampResources()
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

void EditorApp::GpuTimestampBegin(ID3D12GraphicsCommandList* cmd, UINT frameIndex)
{
	const UINT q0 = QueryIndexStart(frameIndex);
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0);
}

void EditorApp::GpuTimestampEndAndResolve(ID3D12GraphicsCommandList* cmd, UINT frameIndex)
{
	const UINT q0 = QueryIndexStart(frameIndex);
	const UINT q1 = q0 + 1;
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q1);

	// 시작/끝 2개를 readback 버퍼에 연속으로 Resolve
	const UINT64 dstOffsetBytes = sizeof(UINT64) * q0;
	cmd->ResolveQueryData(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0, 2, m_tsReadback.Get(), dstOffsetBytes);
}

double EditorApp::ComputeGpuFrameMsAfterCompleted(UINT frameIndex)
{
	if (!m_tsMapped || m_tsFreq == 0) return 0.0;
	const UINT q0 = QueryIndexStart(frameIndex);
	const UINT q1 = q0 + 1;
	const UINT64 t0 = m_tsMapped[q0];
	const UINT64 t1 = m_tsMapped[q1];
	if (t1 <= t0) return 0.0;

	const double sec = double(t1 - t0) / double(m_tsFreq);
	return sec * 1000.0;
}

void EditorApp::RenderFpsUI()
{
	ImGui::Begin("FPS");
	ImGui::Text("FPS : %.3f", GetTimer().GetCpuFPS());
	ImGui::Text("cpu : %.3f ms", GetTimer().GetCpuFrameMsAvg());
	ImGui::Text("gpu : %.3f ms", GetTimer().GetGpuFrameMsAvg());
	ImGui::End();
}

void EditorApp::RenderProfilingUI()
{
	if (auto p = m_profiler.lock())
	{
		auto& snap = p->GetReadSnapshot();
		if (!snap.metrics.empty())
		{
			ImGui::Begin("Profiler");
			for (auto& [name, metric] : snap.metrics)
			{
				ImGui::Text("%s : ", name.c_str());
				const MetricValue& val = metric.value;
				if (std::holds_alternative<double>(val))
				{
					ImGui::SameLine();
					double v = std::get<double>(val);
					ImGui::Text("%.3f", v);
				}
				else if (std::holds_alternative<int64_t>(val))
				{
					ImGui::SameLine();
					int64_t v = std::get<int64_t>(val);
					ImGui::Text(" %lld", v);
				}
				else if (std::holds_alternative<std::string>(val))
				{
					ImGui::SameLine();
					std::string v = std::get<std::string>(val);
					ImGui::Text("%s", v.c_str());
				}
				else if (std::holds_alternative<std::vector<float>>(val))
				{
					const auto& arr = std::get<std::vector<float>>(val);
					if (!arr.empty()) ImGui::PlotLines(name.c_str(), arr.data(), (int)arr.size());
				}
				else if (std::holds_alternative<Histogram>(val))
				{
					const auto& h = std::get<Histogram>(val);
					for (const auto& b : h) {
						ImGui::Text(" %s: %.3f", b.first.c_str(), b.second);
					}
				}
			}
			ImGui::End();
		}

		ImGui::Begin("Buffer Pools");
		ImGui::Text("Pools (Small / Large)");
		ImGui::Separator();

		static bool showPromoted = true;
		ImGui::Checkbox("Show Promoted", &showPromoted);
		ImGui::Separator();
		for (const auto& p : snap.pools)
		{
			double frag = p.free.empty() ? 0.0 : (double)p.free.size();
			ImGui::Text("%s : Capacity: %llu  Used: %llu  Allocated : %zu  FreeBlocks: %zu", p.name.c_str(), (unsigned long long)p.capacity, p.used, p.allocated.size(), p.free.size());

			const float barW = ImGui::GetContentRegionAvail().x;
			const float barH = 14.0f;
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImVec2 p1 = ImVec2(p0.x + barW, p0.y + barH);
			auto* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255), 4.0f);
			dl->AddRect(p0, p1, IM_COL32(200, 200, 200, 128), 4.0f);
			auto toX = [&](UINT64 off)->float { return p0.x + float((double)off / (double)p.capacity * barW); };

			for (auto& block : p.allocated)
			{
				float x0 = toX(block.offset);
				float x1 = toX(block.offset + block.size);
				ImVec2 b0 = ImVec2(x0, p0.y);
				ImVec2 b1 = ImVec2(x1, p1.y);

				dl->AddRectFilled(b0, ImVec2(x1, p1.y), IM_COL32(0, 255, 0, 160), 2.0f);
				dl->AddRect(b0, ImVec2(x1, p1.y), IM_COL32(0, 180, 0, 200), 2.0f);
				if (ImGui::IsMouseHoveringRect(b0, ImVec2(x1, p1.y), false))
				{
					dl->AddRect(b0, ImVec2(x1, p1.y), IM_COL32(255, 220, 0, 255), 3.0f);
					ImGui::SetNextWindowBgAlpha(0.8f);
					ImGui::BeginTooltip();
					if (block.owner)
					{
						ImGui::Text("Owner : %s", block.owner);
					}
					else
					{
						ImGui::Text("<owner unknown>");
					}
					ImGui::Text("Offset: %llu", (unsigned long long)block.offset);
					ImGui::Text("Size  : %llu bytes", (unsigned long long)block.size);
					ImGui::EndTooltip();
				}
			}
			ImGui::Dummy(ImVec2(barW, barH + 6.0f));
		}

		if (showPromoted)
		{
			// TODO : Promoted 리스트 Profiling

		}

		ImGui::End();
	}
}