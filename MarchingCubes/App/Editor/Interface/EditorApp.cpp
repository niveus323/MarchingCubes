#include "pch.h"
#include "EditorApp.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Assets/ResourceManager.h"
#include <numeric>

void EditorApp::OnDestroy()
{
	if (m_commandQueue && m_swapChainFence)
	{
		const uint64_t finalFence = ++m_nextFenceValue;
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
	DrawScene(m_commandList.Get());
	m_uiRenderer->RenderFrame(m_commandList.Get());
	AfterDraw(m_commandList.Get());

	GpuTimestampEndAndResolve(m_commandList.Get(), m_frameIndex);
}

void EditorApp::OnPlatformEvent(uint32_t msg, WPARAM wParam, LPARAM lParam)
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

void EditorApp::OnAfterSwapchainCreated()
{
	InitGpuTimeStampResources();
}

void EditorApp::OnInitPipelines()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//Create Main Root Signature.
	{
		// Define Root Parameter : b0 (CameraBuffer), b1 (ObjectBuffer), b2 (LightBuffer), b3 (TriplanarBuffer) ,t0 (Materials), t1 (EnvMap), t2(TexTable), s0 (LinearSampler)
		CD3DX12_ROOT_PARAMETER1  rootParams[7];
		ZeroMemory(rootParams, sizeof(rootParams));
		rootParams[0].InitAsConstantBufferView(0); // b0
		rootParams[1].InitAsConstantBufferView(1); // b1
		rootParams[2].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2)); // b2
		rootParams[3].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3)); // b3
		rootParams[4].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0)); // t0
		rootParams[5].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1)); // t1
		rootParams[6].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, (uint32_t)-1, 2, 0u, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE)); // t2

		// Static Sampler 등록 ( 런타임에 바꿔야할 샘플러가 필요할 경우 Descriptor Table에 포함할 것.)
		CD3DX12_STATIC_SAMPLER_DESC samplerDescs = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR); // s0

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDescs, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, nullptr));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
		NAME_D3D12_OBJECT(m_rootSignature);
	}

	D3D12_INPUT_LAYOUT_DESC inputLayout = { inputElementDesc, _countof(inputElementDesc) };
	m_renderSystem = std::make_unique<RenderSystem>(m_device.Get(), m_rootSignature.Get(), inputLayout, GetPSOFiles());
}

void EditorApp::OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand)
{
	m_mainCamera = std::make_unique<Camera>(static_cast<float>(m_width), static_cast<float>(m_height));
	m_lightManager = std::make_unique<LightManager>(m_device.Get(), 3);

	if (ResourceManager* resourceManager = GetResourceManager())
	{
		uint32_t sandTexHandle = resourceManager->LoadTexture(GetFullPath(AssetType::Texture, L"gravelly_sand/gravelly_sand_diffuse"));
		uint32_t sandNormalHandle = resourceManager->LoadTexture(GetFullPath(AssetType::Texture, L"gravelly_sand/gravelly_sand_normal"));
		uint32_t sandArmHandle = resourceManager->LoadTexture(GetFullPath(AssetType::Texture, L"gravelly_sand/gravelly_sand_arm"));
		uint32_t sandDispHandle = resourceManager->LoadTexture(GetFullPath(AssetType::Texture, L"gravelly_sand/gravelly_sand_displace"));
		
		Material defaultMatCpu;
		defaultMatCpu.SetMaterialConstants(MaterialConstants{
			.albedo = {1.0f, 1.0f, 1.0f},
			.metallic = 0.0f,
			.specularStrength = 0.04f,
			.roughness = 1.0f,
			.ao = 1.0f,
			.ior = 1.0f,
			.shadingModel = EShadingModel::DefaultLit,
			.opacity = 1.0f
		});
		defaultMatCpu.SetTextureMapping(ETextureMappingTypes::Triplanar);
		defaultMatCpu.SetTriplanarParams(TriplanarParams{ .scale = 0.01f });
		defaultMatCpu.SetDiffuseTex(sandTexHandle);
		defaultMatCpu.SetNormalTex(sandNormalHandle);
		defaultMatCpu.SetArmTex(sandArmHandle);
		defaultMatCpu.SetDisplacementTex(sandDispHandle);

		resourceManager->AddMaterial(defaultMatCpu);
	}

	// 기본 Debug View Mode 등록
	{
		m_hDefaultView = RegisterDebugViewMode("Default", [](RenderSystem* rs) {
			rs->ClearPSOOverrides();
		});

		m_hWireView = RegisterDebugViewMode("Wireframe", [](RenderSystem* rs) {
			rs->ClearPSOOverrides();
			rs->SetPSOOverride("Filled", "Wire");
		});

		m_hNormalView = RegisterDebugViewMode("Visualize Normals", [](RenderSystem* rs) {
			rs->TogglePSOExtension("Filled", "DrawNormal");
		});
	}

	InitScene(initCommand);
	InitUICommon(initCommand);
}

void EditorApp::OnAfterChainSwaped()
{
	// GPU TimeStamp Readback
	const double gpuMs = ComputeGpuFrameMsAfterCompleted(m_frameIndex);
	// Timer에 GPU 프레임 시간(ms) 반영 → 평균/즉시 FPS 계산에 사용
	GetTimer().PushGpuFrameMs(gpuMs);
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
	GpuAllocator* gpuAllocator = GetGpuAllocator();
	StaticBufferRegistry* staticBufferRegistry = GetStaticBufferRegistry();
	if (auto p = m_profiler.lock())
	{
		std::vector<BufferPoolInfo> pools;
		// GpuAllocator
		for (auto& dbg : gpuAllocator->GetDebugPools())
		{
			BufferPoolInfo pi;
			pi.name = dbg.name;
			pi.capacity = dbg.pool->GetCapacity();
			std::vector<BufferBlock>& allocated = dbg.pool->GetAllocatedBlocks();
			pi.used = std::accumulate(allocated.cbegin(), allocated.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) { return sum + b.size; });
			pi.free = dbg.pool->GetFreeBlocks();
			pi.allocated = dbg.pool->GetAllocatedBlocks();
			pools.push_back(pi);
		}

		// StaticBufferRegistry
		BufferPoolInfo pi_vb;
		pi_vb.name = "StaticVB";
		pi_vb.capacity = staticBufferRegistry->GetVBCapacity();
		std::vector<BufferBlock>& allocatedVB = staticBufferRegistry->GetVBAllocated();
		pi_vb.used = std::accumulate(allocatedVB.cbegin(), allocatedVB.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) {return sum + b.size; });
		pi_vb.free = staticBufferRegistry->GetVBFree();
		pi_vb.allocated = allocatedVB;
		pools.push_back(pi_vb);

		BufferPoolInfo pi_ib;
		pi_ib.name = "StaticIB";
		pi_ib.capacity = staticBufferRegistry->GetIBCapacity();
		std::vector<BufferBlock>& allocatedIB = staticBufferRegistry->GetIBAllocated();
		pi_ib.used = std::accumulate(allocatedIB.cbegin(), allocatedIB.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) {return sum + b.size; });
		pi_ib.free = staticBufferRegistry->GetIBFree();
		pi_ib.allocated = allocatedIB;
		pools.push_back(pi_ib);

		p->SetBufferPools(pools);

		p->UpdateFrame(GetTimer().GetTimeMs());
	}
}

void EditorApp::OnUpload(ID3D12GraphicsCommandList* cmd)
{
	GpuTimestampBegin(m_commandList.Get(), m_frameIndex);
	CameraConstants commonCameraData = m_mainCamera->BuildCameraConstants();
	LightBlobView commonLightData = m_lightManager->BuildLightConstants();
	m_renderSystem->PrepareRender(GetUploadContext(), GetDescriptorAllocator(), commonCameraData, commonLightData, m_frameIndex);
}

void EditorApp::DrawScene(ID3D12GraphicsCommandList* cmd)
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

	cmd->RSSetViewports(1, &m_viewport);
	cmd->RSSetScissorRects(1, &m_scissorRect);

	DescriptorAllocator* descriptorAllocator = GetDescriptorAllocator();
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorAllocator->GetCbvSrvUavHeap(), descriptorAllocator->GetSamplerHeap(0) };
	cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//cmd->SetGraphicsRootDescriptorTable(4, descriptorAllocator->GetStaticGpu(m_materialSlot)); // MaterialTable
	if (ResourceManager* resourceManager = GetResourceManager()) resourceManager->BindDescriptorTable(cmd);

	m_renderSystem->RenderFrame(cmd, GetUploadContext());
}

DebugViewModeHandle EditorApp::RegisterDebugViewMode(std::string_view name, std::function<void(RenderSystem*)> func)
{
	for (int i = 0; i < m_debugViewModes.size(); ++i)
	{
		if (m_debugViewModes[i].first == name)
		{
			m_debugViewModes[i].second = func;
			return i;
		}
	}

	m_debugViewModes.push_back({ name.data(), func});
	return static_cast<DebugViewModeHandle>(m_debugViewModes.size() - 1);
}

void EditorApp::SetDebugViewMode(std::string_view name)
{
	for (int i = 0; i < m_debugViewModes.size(); ++i)
	{
		if (m_debugViewModes[i].first == name)
		{
			SetDebugViewMode(i);
			return;
		}
	}

	Log::Print("Editor", "Failed to find debug mode: %s", name);
}

void EditorApp::SetDebugViewMode(int index)
{
	if (index >= 0 && index < m_debugViewModes.size())
	{
		m_currentDebugViewMode = index;
		m_debugViewModes[index].second(m_renderSystem.get());
	}
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

void EditorApp::GpuTimestampBegin(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex)
{
	const uint32_t q0 = QueryIndexStart(frameIndex);
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0);
}

void EditorApp::GpuTimestampEndAndResolve(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex)
{
	const uint32_t q0 = QueryIndexStart(frameIndex);
	const uint32_t q1 = q0 + 1;
	cmd->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q1);

	// 시작/끝 2개를 readback 버퍼에 연속으로 Resolve
	const uint64_t dstOffsetBytes = sizeof(uint64_t) * q0;
	cmd->ResolveQueryData(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, q0, 2, m_tsReadback.Get(), dstOffsetBytes);
}

double EditorApp::ComputeGpuFrameMsAfterCompleted(uint32_t frameIndex)
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
			auto toX = [&](uint64_t off)->float { return p0.x + float((double)off / (double)p.capacity * barW); };

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
					if (!block.owner.empty())
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