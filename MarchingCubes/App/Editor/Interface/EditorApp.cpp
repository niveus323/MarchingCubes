#include "pch.h"
#include "EditorApp.h"
#include "Win32Application.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/UI/UIRenderer.h"
#include "Core/Trace/Profiler.h"
#include "Core/Input/InputState.h"
#include <numeric>
using namespace std::placeholders;

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

	// UI용 힙과 폰트용 슬롯을 할당
	DescriptorAllocator* descriptorAllocator = GetDescriptorAllocator();
	ID3D12DescriptorHeap* mainHeap = descriptorAllocator->GetCbvSrvUavHeap();
	uint32_t fontSlot = descriptorAllocator->AllocateStaticSlot();

	ImGUIInitOptions initOptions = {};
	initOptions.commandQueue = GetPresentQueue();
	initOptions.nums_of_frame = kFrameCount;
	initOptions.srvHeap = mainHeap;
	initOptions.cpuHandle = descriptorAllocator->GetStaticCpu(fontSlot);
	initOptions.gpuHandle = descriptorAllocator->GetStaticGpu(fontSlot);
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

	m_uiToken_Fps = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderFpsUI, this, _1), UI::UICallbackOptions{
		.layer = UI::EUILayer::Global_Debug,
		.rateHz = 0,
		.enabled = true,
		.id = "Fps"
		});

	m_hierarchyPanel = std::make_unique<SceneHierarchyPanel>();
	m_uiToken_Hierarchy = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&EditorApp::RenderHierarchyUI, this, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "SceneHierarchy"
		}
	);

	m_inspectorPanel = std::make_unique<InspectorPanel>();
	m_uiToken_Inspector = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&EditorApp::RenderInspectorUI, this, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "Inspector"
		}
	);

	// TODO : Profiler 에디터로 옮기기
	m_uiToken_Profiler = m_uiRenderer->AddFrameRenderCallbackToken(std::bind(&EditorApp::RenderProfilingUI, this, _1), UI::UICallbackOptions{
		.layer = UI::EUILayer::Global_Debug,
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
#ifdef _DEBUG
	GpuAllocator* gpuAllocator = GetGpuAllocator();
	StaticBufferRegistry* staticBufferRegistry = GetStaticBufferRegistry();
	if (auto p = m_profiler.lock())
	{
		std::vector<BufferPoolInfo> poolInfos;
		std::vector<DedicatedBufferInfo> promotedInfos;
		// GpuAllocator
		for (auto& dbg : gpuAllocator->GetDebugPools())
		{
			BufferPoolInfo poolInfo;
			poolInfo.name = dbg.name;
			poolInfo.capacity = dbg.pool->GetCapacity();
			std::vector<BufferBlock>& allocated = dbg.pool->GetAllocatedBlocks();
			poolInfo.used = std::accumulate(allocated.cbegin(), allocated.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) { return sum + b.size; });
			poolInfo.free = dbg.pool->GetFreeBlocks();
			poolInfo.allocated = dbg.pool->GetAllocatedBlocks();
			poolInfos.push_back(poolInfo);
		}

		// StaticBufferRegistry
		BufferPoolInfo pi_vb;
		pi_vb.name = "StaticVB";
		pi_vb.capacity = staticBufferRegistry->GetVBCapacity();
		std::vector<BufferBlock>& allocatedVB = staticBufferRegistry->GetVBAllocated();
		pi_vb.used = std::accumulate(allocatedVB.cbegin(), allocatedVB.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) {return sum + b.size; });
		pi_vb.free = staticBufferRegistry->GetVBFree();
		pi_vb.allocated = allocatedVB;
		poolInfos.push_back(pi_vb);

		BufferPoolInfo pi_ib;
		pi_ib.name = "StaticIB";
		pi_ib.capacity = staticBufferRegistry->GetIBCapacity();
		std::vector<BufferBlock>& allocatedIB = staticBufferRegistry->GetIBAllocated();
		pi_ib.used = std::accumulate(allocatedIB.cbegin(), allocatedIB.cend(), 0ULL, [](uint64_t sum, const BufferBlock& b) {return sum + b.size; });
		pi_ib.free = staticBufferRegistry->GetIBFree();
		pi_ib.allocated = allocatedIB;
		poolInfos.push_back(pi_ib);

		p->SetBufferPools(poolInfos);
		p->SetDedicatedBuffers(gpuAllocator->GetDebugDedicatedBuffers());
		p->UpdateFrame(GetTimer().GetTimeMs());
	}
#endif
}

void EditorApp::OnSceneLoaded(Scene* scene)
{
	if (scene)
	{
		scene->BeginEditor();

		if(m_hierarchyPanel) m_hierarchyPanel->SetCurrentScene(scene);
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


void EditorApp::RenderFpsUI(IUIBuilder* ui)
{
	ui->BeginPanel("FPS");
	ui->Text(std::format("FPS : {:.3f}", GetTimer().GetCpuFPS()));
	ui->Text(std::format("cpu : {:.3f} ms", GetTimer().GetCpuFrameMsAvg()));
	ui->Text(std::format("gpu : {:.3f} ms", GetTimer().GetGpuFrameMsAvg()));
	ui->EndPanel();
}

void EditorApp::RenderHierarchyUI(IUIBuilder* ui)
{
	if (m_hierarchyPanel && m_currentScene)
	{
		m_hierarchyPanel->OnRenderUI(ui);
		m_selectedObject = m_hierarchyPanel->GetSelectedObject();
		if (m_inspectorPanel) m_inspectorPanel->SetTarget(m_selectedObject);
	}
}

void EditorApp::RenderInspectorUI(IUIBuilder* ui)
{
	if (m_inspectorPanel)
	{
		m_inspectorPanel->OnRenderUI(ui);
	}
}

void EditorApp::RenderProfilingUI(IUIBuilder* ui)
{
	if (auto p = m_profiler.lock())
	{
		auto& snap = p->GetReadSnapshot();
		if (!snap.metrics.empty())
		{
			ui->BeginPanel("Profiler");
			for (auto& [name, metric] : snap.metrics)
			{
				ui->Text(std::format("{} : ", name));
				const MetricValue& val = metric.value;
				if (std::holds_alternative<double>(val))
				{
					ui->SameLine();
					ui->Text(std::format("{:.3f}", std::get<double>(val)));
				}
				else if (std::holds_alternative<int64_t>(val))
				{
					ui->SameLine();
					ui->Text(std::format("{}", std::get<int64_t>(val)));
				}
				else if (std::holds_alternative<std::string>(val))
				{
					ui->SameLine();
					ui->Text(std::format("{}", std::get<std::string>(val)));
				}
				else if (std::holds_alternative<std::vector<float>>(val))
				{
					const auto& arr = std::get<std::vector<float>>(val);
					if (!arr.empty()) ui->PlotLines(name.c_str(), arr.data(), (int)arr.size());
				}
				else if (std::holds_alternative<Histogram>(val))
				{
					const auto& h = std::get<Histogram>(val);
					for (const auto& b : h) 
					{
						ui->Text(std::format(" {}: {:.3f}", b.first, b.second));
					}
				}
			}
			ui->EndPanel();
		}

		if (ui->BeginPanel("Buffer Pools"))
		{
			ui->Text("Pools (Small / Large)");
			ui->Separator();

			static bool showPromoted = true;
			ui->Checkbox("Show Promoted", &showPromoted);
			ui->Separator();
			for (const auto& p : snap.pools)
			{
				ui->Text(std::format("{} : Capacity: {}  Used: {}  Allocated : {}  FreeBlocks: {}", p.name.c_str(), (unsigned long long)p.capacity, p.used, p.allocated.size(), p.free.size()));

				const float barW = ui->GetAvailableWidth();
				const float barH = 14.0f;
				UI::Vector<float, 2> p0 = ui->GetCursorScreenPos();
				UI::Vector<float, 2> p1 = UI::Vector<float, 2>(p0.x + barW, p0.y + barH);
				ui->DrawRectFilled(p0, p1, { 0.12f, 0.12f, 0.12f, 1.0f });
				ui->DrawRect(p0, p1, { 0.8f, 0.8f, 0.8f, 0.5f });

				auto toX = [&](uint64_t off)->float { return p0.x + float((double)off / (double)p.capacity * barW); };
				for (auto& block : p.allocated)
				{
					float x0 = toX(block.offset);
					float x1 = toX(block.offset + block.size);
					UI::Vector<float, 2> b0 = UI::Vector<float, 2>(x0, p0.y);
					UI::Vector<float, 2> b1 = UI::Vector<float, 2>(x1, p1.y);

					ui->DrawRectFilled(b0, b1, { 0.0f, 1.0f, 0.0f, 0.6f });
					ui->DrawRect(b0, b1, { 0.0f, 0.7f, 0.0f, 0.8f });
					if (ui->IsMouseHoveringRect(b0, b1))
					{
						ui->DrawRect(b0, b1, { 1.0f, 0.86f, 0.0f, 1.0f });

						ui->BeginTooltip();
						if (!block.owner.empty())
						{
							ui->Text(std::format("Owner : {}", block.owner).c_str());
						}
						else
						{
							ui->Text("<owner unknown>");
						}
						ui->Text(std::format("Offset: {}", block.offset).c_str());
						ui->Text(std::format("Size  : {} bytes", block.size).c_str());
						ui->EndTooltip();
					}
				}
				ui->Dummy({ barW, barH + 6.0f });
			}

			if (showPromoted)
			{
				ui->Separator();
				ui->Text("Dedicated Allocations (Fallback / Promoted)");

				if (ui->BeginTable("DedicatedAllocTable", 5))
				{
					ui->TableNextColumn();
					ui->EndTable();
				}

				//if (ImGui::BeginTable("DedicatedAllocTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
				if (ui->BeginTable("DedicatedAllocTable", 5))
				{
					ui->TableSetupColumn("Type");
					ui->TableSetupColumn("Owner");
					ui->TableSetupColumn("Size");
					ui->TableSetupColumn("Usage");
					ui->TableSetupColumn("Status");
					ui->TableHeadersRow();

					for (const auto& info : snap.dedicatedBuffers)
					{
						ui->TableNextRow();

						ui->TableNextColumn();
						ui->Text(info.type.c_str());

						ui->TableNextColumn();
						// Owner가 비어있으면 Unknown 표시
						ui->Text(info.owner.empty() ? "-" : info.owner.c_str());

						ui->TableNextColumn();
						if (info.size > 1024 * 1024)
							ui->TextFormatted("%.2f MB", info.size / (1024.0f * 1024.0f));
						else
							ui->TextFormatted("%.2f KB", info.size / 1024.0f);

						ui->TableNextColumn();
						ui->Text(info.usage.c_str());

						ui->TableNextColumn();
						if (info.isLive)
							ui->TextColored(UI::Color(0, 1, 0, 1), "Live");
						else
							ui->TextColored(UI::Color(1, 1, 0, 1), "Pending (Fence: %llu)", info.fenceValue);
					}
					ui->EndTable();
				}
			}

			ui->EndPanel();
		}
	}
}

void EditorApp::OnPlayButtonClicked()
{
	if (m_currentScene)
	{
		m_currentScene->EndEditor();
		m_currentScene->BeginPlay();
	}
}

void EditorApp::OnCloseButtonClicked()
{
	if (m_currentScene)
	{
		m_currentScene->EndPlay();
		m_currentScene->BeginEditor();
	}
}
