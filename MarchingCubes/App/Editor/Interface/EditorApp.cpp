#include "pch.h"
#include "EditorApp.h"
#include "Win32Application.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/UI/UIRenderer.h"
#include "Core/Trace/Profiler.h"
#include "Core/Rendering/Memory/GpuAllocator.h"
#include "Core/Input/InputState.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Engine/Serializer/JsonSerializer.h"
#include "Core/Utils/FileUtils.h"
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
	if (m_inputState->GetKeyState(ActionKey::ToggleDebugView) == ActionKeyState::JustReleased)
	{
		SetDebugViewMode(m_hDefaultView);
	}
	else if (m_inputState->GetKeyState(ActionKey::ToggleWireFrame) == ActionKeyState::JustReleased)
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
	else if (m_inputState->GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustReleased)
	{
		SetDebugViewMode(m_hNormalView); // Just Toggle
	}
#endif // _DEBUG

}

void EditorApp::OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand)
{
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
	
	// Main Menu Bar 등록 (화면 최상단)
	m_uiToken_MainMenuBar = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&EditorApp::RenderMainMenuBarUI, this, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel, // 혹은 별도의 최상위 레이어
			.enabled = true,
			.id = "MainMenuBar"
		}
	);

	// Subsystem Manager 패널 등록
	m_uiToken_SubsystemManager = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&EditorApp::RenderSubsystemManagerUI, this, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "SubsystemManager"
		}
	);

	m_hierarchyPanel = AddPanel<SceneHierarchyPanel>();
	m_hierarchyPanel->SetOnSelectionChanged([&](GameObject* selected){
		if (m_currentScene)
		{
			if (auto controller = dynamic_cast<EditorController*>(m_currentScene->GetController()))
			{
				controller->SelectObject(selected);
			}
		}
		if (m_inspectorPanel) m_inspectorPanel->SetTarget(selected);
	});

	m_uiToken_Hierarchy = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&EditorApp::RenderHierarchyUI, this, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "SceneHierarchy"
		}
	);

	m_inspectorPanel = AddPanel<InspectorPanel>();
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
#ifdef _DEBUG
	GpuAllocator* gpuAllocator = GetGpuAllocator();
	if (gpuAllocator)
	{
		if (auto p = m_profiler.lock())
		{
			p->SetBufferPools(gpuAllocator->GetMemoryInfos());
			p->SetDedicatedBuffers(gpuAllocator->GetDebugDedicatedBuffers());
			p->UpdateFrame(GetTimer().GetTimeMs());
		}
	}
#endif
}

void EditorApp::OnSceneLoaded(Scene* scene)
{
	if (scene)
	{
		scene->BeginEditor();
		if(m_hierarchyPanel) m_hierarchyPanel->SetCurrentScene(scene);
		if(m_inspectorPanel) m_inspectorPanel->SetTarget(nullptr);

		if (auto controller = dynamic_cast<EditorController*>(scene->GetController()))
		{
			controller->SetSelectionChangedCallback([this](GameObject* newSelection) {
				if (m_hierarchyPanel) m_hierarchyPanel->SetSelection(newSelection);
				if (m_inspectorPanel) m_inspectorPanel->SetTarget(newSelection);
			});
		}
	}
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
	}
}

void EditorApp::RenderInspectorUI(IUIBuilder* ui)
{
	if (m_inspectorPanel && m_currentScene)
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
				ui->Text("Dedicated Allocations Fallback");

				if (ui->BeginTable("DedicatedAllocTable", 5))
				{
					ui->TableNextColumn();
					ui->EndTable();
				}

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
		}
		ui->EndPanel();
	}
}

void EditorApp::RenderMainMenuBarUI(IUIBuilder* ui)
{
	ui->BeginMainMenuBar();
	// 파일 로드/세이브
	if (ui->BeginMenu("File"))
	{
		if (ui->MenuItem("Save Scene"))
		{
			std::string filepath = FileUtils::FileDialogs::SaveFile("JSON Scene File", "*.json");
			if (!filepath.empty())
			{
				JsonSerializer ar(true);
				m_currentScene->Serialize(ar);
				ar.WriteToFile(filepath);
			}
		}
		if (ui->MenuItem("Load Scene"))
		{
			std::string filepath = FileUtils::FileDialogs::OpenFile("JSON Scene File", "*.json");
			if (!filepath.empty()) RequestLoadScene(filepath);
		}
		ui->EndMenu();
	}

	// 입력 키 세팅, 환경설정, 서브시스템 관리 등
	if (ui->BeginMenu("Edit"))
	{
		if (ui->MenuItem("Subsystem Manager"))
		{
			// 패널 표시 플래그 토글
			m_bShowSubsystemManager = true;
		}
		ui->EndMenu();
	}

	// Panel, Editor Tool 등 에디터 작업을 위한 도구
	if (ui->BeginMenu("Tool"))
	{
		bool bHierarchyVisible = m_hierarchyPanel ? m_hierarchyPanel->IsPanelVisible() : false;
		if (ui->MenuItem("Scene Hierarchy", nullptr, bHierarchyVisible))
		{
			if (m_hierarchyPanel) m_hierarchyPanel->SetPanelVisible(!bHierarchyVisible);
		}

		bool bInspectorVisible = m_inspectorPanel ? m_inspectorPanel->IsPanelVisible() : false;
		if (ui->MenuItem("Inspector", nullptr, bInspectorVisible))
		{
			if (m_inspectorPanel) m_inspectorPanel->SetPanelVisible(!bInspectorVisible);
		}
		ui->Separator();
		
		// Editor Tools
		static std::vector<TypeDescriptor*> toolTypes;
		static bool bToolTypesLoaded = false;
		if (!bToolTypesLoaded)
		{
			toolTypes = ReflectionRegistry::Get().GetTypesDerivedFrom("IEditorTool");
			bToolTypesLoaded = true;
		}

		EditorController* editorController = nullptr;
		if (m_currentScene)
		{
			editorController = dynamic_cast<EditorController*>(m_currentScene->GetController());
		}

		for (int i=0; i<toolTypes.size(); ++i)
		{
			TypeDescriptor* desc = toolTypes[i];
			std::string toolName = desc->GetName();
			if (toolName == IEditorTool::GetStaticType()->GetName()) continue;

			bool bIsSelected = false;
			if (editorController)
			{
				auto activeTool = editorController->GetActiveTool();
				if (activeTool && activeTool->GetType()->GetName() == toolName) bIsSelected = true;
			}

			if (ui->MenuItem(toolName.c_str(), NULL, bIsSelected))
			{
				if (!m_currentScene) continue;

				if (editorController)
				{
					if (bIsSelected)
					{
						editorController->SetTool(nullptr);
					}
					else
					{
						if (IEditorTool* rawTool = static_cast<IEditorTool*>(desc->CreateInstance()))
						{
							std::shared_ptr<IEditorTool> newTool(rawTool);
							editorController->SetTool(newTool);
						}
					}
				}
			}
		}
		ui->EndMenu();
	}

	ui->EndMainMenuBar();
}

void EditorApp::RenderSubsystemManagerUI(IUIBuilder* ui)
{
	if (!m_bShowSubsystemManager) return;

	if (ui->BeginPanel("Subsystem Manager", &m_bShowSubsystemManager))
	{
		if (m_currentScene)
		{
			// 서브시스템 이름 사전 구축
			static std::vector<std::string> s_subsystemNames;
			if (s_subsystemNames.empty())
			{
				std::vector<TypeDescriptor*> types = ReflectionRegistry::Get().GetTypesDerivedFrom("ISceneSubsystem");
				for (TypeDescriptor* desc : types)
				{
					std::string name = desc->GetName();
					// 인터페이스는 제외하고 사전에 추가
					if (name != ISceneSubsystem::GetStaticType()->GetName()) s_subsystemNames.push_back(name);
				}
			}

			static std::vector<int> availableItems;
			static std::vector<int> basketItems;
			// 씬이 새로 로드되거나 변경되었을 때 1회 동기화 (Scene -> UI)
			static Scene* lastScene = nullptr;
			if (lastScene != m_currentScene.get())
			{
				availableItems.clear();
				basketItems.clear();

				// 현재 씬에 등록된 서브시스템 이름들 수집
				auto& activeSubsystems = m_currentScene->GetSubsystems();
				std::vector<std::string> activeNames;
				for (const auto& [type, subsys] : activeSubsystems)
				{
					activeNames.push_back(subsys->GetType()->GetName());
				}

				// 전체 서브시스템을 순회하며 씬 등록 여부에 따라 Available / Basket 분류
				for (int i = 0; i < s_subsystemNames.size(); ++i)
				{
					auto it = std::find(activeNames.begin(), activeNames.end(), s_subsystemNames[i]);
					if (it != activeNames.end()) basketItems.push_back(i);
					else availableItems.push_back(i);
				}
				lastScene = m_currentScene.get();
			}

			auto SubsysIdxToName = [](int id) ->std::string {
				if (id >= 0 && id < s_subsystemNames.size())
					return s_subsystemNames[id];
				return "Unknown";
			};

			if (ui->DualListBox("SubsystemManagerDualList", availableItems, basketItems, SubsysIdxToName))
			{
				// 씬에 등록되어 있는 서브시스템 리스트 확인
				auto& activeSubsystems = m_currentScene->GetSubsystems();
				std::vector<std::string> activeNames;
				for (const auto& [type, subsys] : activeSubsystems)
				{
					activeNames.push_back(subsys->GetType()->GetName());
				}

				// 서브시스템 추가
				for (int id : basketItems)
				{
					const std::string& name = s_subsystemNames[id];
					if (std::find(activeNames.begin(), activeNames.end(), name) == activeNames.end())
					{
						m_currentScene->AddSubsystemByName(name);
					}
				}

				// 서브시스템 해제
				std::vector<std::type_index> typesToRemove;
				for (const auto& [type, subsys] : activeSubsystems)
				{
					std::string name = subsys->GetType()->GetName();
					bool bFoundInBasket = false;
					for (int id : basketItems)
					{
						if (s_subsystemNames[id] == name)
						{
							bFoundInBasket = true;
							break;
						}
					}

					if (!bFoundInBasket) typesToRemove.push_back(type);
				}

				for (const auto& type : typesToRemove)
				{
					m_currentScene->RemoveSubsystem(type);
				}
			}
		}
		else
		{
			ui->Text("No active scene loaded.");
		}

		ui->EndPanel();
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
