#include "pch.h"
#include "EditorApp.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Trace/Profiler.h"
#include "Core/Input/InputState.h"
#include <numeric>

void EditorApp::OnDestroy()
{
	DXAppBase::OnDestroy();
}

void EditorApp::OnUpdate(float deltaTime)
{
	if (m_inputState->IsPressed(ActionKey::Escape))
	{
		PostQuitMessage(0);
		return;
	}

#ifdef _DEBUG
	if (m_inputState->GetKeyState(ActionKey::ToggleDebugView) == ActionKeyState::JustPressed)
	{
		SetDebugViewMode(m_hDefaultView);
	}
	else if (m_inputState->GetKeyState(ActionKey::ToggleWireFrame) == ActionKeyState::JustPressed)
	{
		if (m_renderSystem->IsOverrideActive("Filled", "Wire"))
		{
			SetDebugViewMode(m_hDefaultView);
		}
		else
		{
			SetDebugViewMode(m_hWireView);
		}
	}
	else if (m_inputState->GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustPressed)
	{
		SetDebugViewMode(m_hNormalView); // Just Toggle
	}
#endif // _DEBUG

}

void EditorApp::OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand)
{
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
}

void EditorApp::InitUI(ID3D12GraphicsCommandList* cmd)
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

	m_uiToken_Fps = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderFpsUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Fps"
		});

	// TODO : Profiler 에디터로 옮기기
	m_uiToken_Profiler = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderProfilingUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Profiler"
		});

	// profiler
	m_profilerOwner = std::make_shared<Profiler>();
	m_profiler = m_profilerOwner;

	DXAppBase::InitUI(cmd);
}

void EditorApp::OnUpdateUI(float deltaTime)
{
	// TODO : Profiler 에디터로 옮기기
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

void EditorApp::CreateRootSignature()
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

void EditorApp::CreateInputElements()
{
	m_inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
		.SemanticName = "POSITION",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = 0,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	});

	m_inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ 
		.SemanticName = "NORMAL",   
		.SemanticIndex = 0, 
		.Format = DXGI_FORMAT_R32G32B32_FLOAT,    
		.InputSlot = 0, 
		.AlignedByteOffset= 12, 
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 
		.InstanceDataStepRate= 0 
	});
	
	m_inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
		.SemanticName = "TANGENT",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = 24,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	});

	m_inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
		.SemanticName = "TEXCOORD",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = 40,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	});

	m_inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
		.SemanticName = "COLOR",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = 48,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	});
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