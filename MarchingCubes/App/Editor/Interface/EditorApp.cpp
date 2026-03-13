#include "pch.h"
#include "EditorApp.h"
#include "Win32Application.h"
#include "Core/UI/Renderer/ImGUIRenderer.h"
#include "Core/Trace/Profiler.h"
#include "Core/Rendering/Memory/GpuAllocator.h"
#include "Core/Input/InputState.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Engine/Serializer/JsonSerializer.h"
#include "Core/Utils/FileUtils.h"
#include "Core/Scene/Object/Pawn.h"
#include "App/Editor/Panel/ContentBrowserPanel.h"
constexpr uint32_t kEditorMenuBarHeight = 19u;
using namespace std::placeholders;

EditorApp::EditorApp(uint32_t width, uint32_t height, std::wstring name) :
	DXAppBase(width, height + kEditorMenuBarHeight, name)
{
}

void EditorApp::Destroy()
{
	DXAppBase::Destroy();
}

void EditorApp::Update(float deltaTime)
{
	if (m_bResizePending)
	{
		WaitForGpu();
		OnResizeViewport(m_pendingViewportSize);
		m_bResizePending = false;
	}

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
		if (m_renderSystem->IsOverrideActive("Filled", "Wire")) SetDebugViewMode(m_hDefaultView);
		else SetDebugViewMode(m_hWireView);
	}
	else if (m_inputState->GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustReleased)
	{
		SetDebugViewMode(m_hNormalView); // Just Toggle
	}
#endif // _DEBUG

	if (m_currentScene && m_currentScene->IsPlaying())
	{
		if (m_inputState->IsPressedOnce(ActionKey::Eject))
		{
			m_currentScene->ToggleEject();
			bool currentEject = m_currentScene->IsEjected();
			if (m_viewportPanel)
			{
				m_viewportPanel->SetEditorController(currentEject ? m_currentScene->GetEditorController() : nullptr);
				if (currentEject) m_viewportPanel->AddOnScreenDebugMessage("Ejected");
			}
		}
	}
}

void EditorApp::UpdateUI(float deltaTime)
{
	for (auto& panel : m_editorPanels)
	{
		panel->OnUpdate(deltaTime);
	}

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
}

D3D12_GPU_DESCRIPTOR_HANDLE EditorApp::GetOffscreenSRVGpuHandle()
{
	if (m_offscreenSRVHandle == UINT32_MAX) return { 0 };
	return m_descriptorAllocator->GetStaticGpu(m_offscreenSRVHandle);
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

	// Dockspace
	m_uiRenderer->AddFrameRenderCallbackToken(
		[](IUIBuilder* ui) {
			ui->DockSpaceOverViewport("Main DockSpace");
		},
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Background,
			.enabled = true,
			.id = "MainDockSpace"
		}
	);

	// --- Editor Panel ---
	m_viewportPanel = AddPanel<ViewportPanel>();
	m_viewportPanel->SetOnViewportChanged(std::bind(&EditorApp::RequestResizeViewport, this, _1));
	m_uiToken_Viewport = m_uiRenderer->AddFrameRenderCallbackToken(
		std::bind(&ViewportPanel::OnRenderUI, m_viewportPanel, _1),
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "Viewport"
		}
	);

	m_hierarchyPanel = AddPanel<SceneHierarchyPanel>();
	m_hierarchyPanel->SetOnSelectionChanged([&scene = m_currentScene, &inspector = m_inspectorPanel](GameObject* selected) {
		if (scene)
		{
			if (auto controller = scene->GetEditorController()) controller->SelectObject(selected);
		}
		if (inspector) inspector->SetTarget(selected);
		});

	m_uiToken_Hierarchy = m_uiRenderer->AddFrameRenderCallbackToken(
		[&scene = m_currentScene, &panel = m_hierarchyPanel](IUIBuilder* ui) {
			if (!scene || !panel) return;
			panel->OnRenderUI(ui);
		},
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "SceneHierarchy"
		}
	);

	m_inspectorPanel = AddPanel<InspectorPanel>();
	m_uiToken_Inspector = m_uiRenderer->AddFrameRenderCallbackToken(
		[&scene = m_currentScene, &panel = m_inspectorPanel](IUIBuilder* ui) {
			if (!scene || !panel) return;
			panel->OnRenderUI(ui);
		},
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Panel,
			.enabled = true,
			.id = "Inspector"
		}
	);

	auto conentBrowserPanel = AddPanel<ContentBrowserPanel>();
	m_uiRenderer->AddFrameRenderCallbackToken([&](IUIBuilder* ui) {
			if (auto panel = GetPanel<ContentBrowserPanel>()) panel->OnRenderUI(ui);
		},
		UI::UICallbackOptions{
			.layer = UI::EUILayer::Editor_Window,
			.enabled = true,
			.id = "Content_Browser"
		}
	);
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

void EditorApp::RenderFrame(ID3D12GraphicsCommandList* cmd)
{
	// 3D 씬 렌더링 (오프스크린 타겟)
	if (m_offscreenResource)
	{
		const auto offscreenToRT = CD3DX12_RESOURCE_BARRIER::Transition(m_offscreenResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->ResourceBarrier(1, &offscreenToRT);

		DXAppBase::RenderScene(cmd);

		D3D12_RESOURCE_BARRIER afterRenderScene[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(m_offscreenResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE), // 오프스크린 텍스처를 ImGui가 읽을 수 있도록 SRV 상태로 변경
			CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackbuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) 	// 백 버퍼 준비 (UI 렌더링용)
		};
		cmd->ResourceBarrier(2, afterRenderScene);
	}
	else
	{
		const auto backbufferToRT = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackbuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->ResourceBarrier(1, &backbufferToRT);
	}
	
	auto backBufferRTV = m_descriptorAllocator->GetRTVCpu(m_rtvHandles[m_frameIndex]);
	cmd->OMSetRenderTargets(1, &backBufferRTV, FALSE, nullptr); // 깊이 버퍼 불필요
	
	const float editorBgColor[] = { 0.15f, 0.15f, 0.15f, 1.0f };
	cmd->ClearRenderTargetView(backBufferRTV, editorBgColor, 0, nullptr);
}

void EditorApp::OnAfterSwapchainCreated()
{
	if (m_bOffscreenHandlesAllocated)
	{
		auto rtvCpu = m_descriptorAllocator->GetRTVCpu(m_offscreenRTVHandle);
		auto dsvCpu = m_descriptorAllocator->GetDSVCpu(m_offscreenDSVHandle);
		m_renderSystem->SetOutputTarget(m_offscreenResource.Get(), rtvCpu, dsvCpu);
	}
	
	UI::Vector<float, 2> viewportSize = (m_viewportPanel) ? m_viewportPanel->GetViewportSize() : UI::Vector<float,2>{ static_cast<float>(m_width), static_cast<float>(m_height) };
	m_renderSystem->SetViewport(0.0f, 0.0f, viewportSize.x, viewportSize.y);
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

void EditorApp::OnSceneLoaded(Scene* scene)
{
	if (scene)
	{
		scene->BeginEditor();
		if(m_hierarchyPanel) m_hierarchyPanel->SetCurrentScene(scene);
		if(m_inspectorPanel) m_inspectorPanel->SetTarget(nullptr);

		if (auto editorController = scene->GetEditorController())
		{
			if (m_viewportPanel) m_viewportPanel->SetEditorController(editorController);
			editorController->SetSelectionChangedCallback([this](GameObject* newSelection) {
				if (m_hierarchyPanel) m_hierarchyPanel->SetSelection(newSelection);
				if (m_inspectorPanel) m_inspectorPanel->SetTarget(newSelection);
			});
		}
	}
}

void EditorApp::UpdateInputCaptureState()
{
	if (!m_currentScene) return;
	bool bPrevInputCaptured = m_bInputCaptured;

	// 게임 플레이 중 eject를 하면 캡쳐 해제
	if (m_currentScene->IsEjected())
	{
		m_bInputCaptured = false;
	}

	// 플레이 중 Shift + F1을 누르면 마우스 캡쳐 해제
	if (m_bInputCaptured)
	{
		bool bShiftPressed = m_inputState->IsPressed(ActionKey::Shift);
		bool bF1Pressed = m_inputState->IsPressed(ActionKey::ToggleDebugView);
		if (bShiftPressed && bF1Pressed)
		{
			m_bInputCaptured = false;
			m_inputState->SetGameInputActive(false);
		}
	}

	// 캡처 재진입 로직 (플레이 모드 중 뷰포트 클릭)
	if (m_currentScene->IsPlaying() && !m_bInputCaptured)
	{
		if (m_viewportPanel && m_viewportPanel->IsHovered() && m_inputState->IsPressedOnce(ActionKey::LeftClick))
		{
			m_bInputCaptured = true;
			m_inputState->SetGameInputActive(true);
			// Eject상태에서 재진입 시 Eject 해제
			if (m_currentScene->IsEjected())
			{
				m_currentScene->ToggleEject();
				m_viewportPanel->SetEditorController(nullptr);
			}
		}
	}

	if (m_bInputCaptured != bPrevInputCaptured)
	{
		Win32Application::CaptureMouseInScreen(m_bInputCaptured);
	}

	bool bBlockMouse = false;
	bool bBlockKeyboard = false;

	if (m_viewportPanel && m_uiRenderer)
	{
		if (m_viewportPanel->IsHovered() && !m_uiRenderer->IsMouseInteracting())
		{
			bBlockMouse = false;
		}
		else
		{
			bBlockMouse = m_uiRenderer->IsCapturingMouse();
		}

		if (m_viewportPanel->IsFocused() && !m_uiRenderer->IsKeyboardInteracting())
		{
			bBlockKeyboard = false;
		}
		else
		{
			bBlockKeyboard = m_uiRenderer->IsCapturingKeyboard();
		}
	}

	if (m_bInputCaptured)
	{
		bBlockMouse = false;
		bBlockKeyboard = false;
	}

	m_inputState->SetInputBlocked(bBlockMouse, bBlockKeyboard);
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
	ui->BeginDisabled(m_currentScene->IsPlaying());

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

		if (ui->BeginMenu("Viewport Options"))
		{
			EditorController* editorController = m_currentScene->GetEditorController();
			float camSpeed = editorController->GetCameraSpeed();
			if (ui->Drag("Camera Speed", &camSpeed, 100.0f, 1.0f)) editorController->SetCameraSpeed(camSpeed);

			float gizmoSize = editorController->GetGizmoSize();
			if (ui->Drag("Gizmo Size", &gizmoSize, 100.0f, 0.01f)) editorController->SetGizmoSize(gizmoSize);

			ui->EndMenu();
		}

		if (ui->MenuItem("Input"))
		{

		}

		ui->EndMenu();
	}

	if (ui->BeginMenu("GameObject"))
	{
		if (ui->MenuItem("GameObject")) m_currentScene->CreateObject<GameObject>("GameObject");
		if (ui->MenuItem("SceneObject")) m_currentScene->CreateObject<SceneObject>("SceneObject");
		if (ui->MenuItem("Pawn")) m_currentScene->CreateObject<Pawn>("NewPawn");

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

		if (auto contentBrowser = GetPanel<ContentBrowserPanel>())
		{
			bool bIsVisible = contentBrowser->IsPanelVisible();
			if (ui->MenuItem("Content Browser", nullptr, bIsVisible))
			{
				contentBrowser->SetPanelVisible(!bIsVisible);
			}
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

		EditorController* editorController = m_currentScene->GetEditorController();
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

	ui->EndDisabled();
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
		m_bInputCaptured = true;
		m_inputState->SetGameInputActive(true);
		ImGui::SetWindowFocus(nullptr);

		JsonSerializer ar(true);
		m_currentScene->Serialize(ar);
		ar.WriteToFile("TempPIE_Backup.json"); //TODO : 임시 파일 폴더에 저장

		m_viewportPanel->SetEditorController(nullptr);
		m_currentScene->BeginPlay();

		// 마우스 캡쳐
		Win32Application::CaptureMouseInScreen(m_bInputCaptured);
	}
}

void EditorApp::OnStopButtonClicked()
{
	if (m_currentScene)
	{
		m_bInputCaptured = false;
		m_inputState->SetGameInputActive(false);
		m_currentScene->EndPlay();
		RequestLoadScene("TempPIE_Backup.json"); //OnSceneLoaded를 통해 에디터 실행 등 초기 설정 실행됨

		// NOTE : Stop 버튼을 눌른 상황은 이미 Play 모드에서 마우스 캡쳐를 해제한 상황이므로 CaptureMouseInScreen을 중복 호출하지 않음
	}
}

void EditorApp::RequestResizeViewport(UI::Vector<float, 2> viewportSize)
{
	m_bResizePending = true;
	m_pendingViewportSize = viewportSize;
}

void EditorApp::OnResizeViewport(UI::Vector<float, 2> viewportSize)
{
	auto allocator = EngineCore::GetDescriptorAllocator();
	float renderWidth = viewportSize.x;
	float renderHeight = viewportSize.y;

	if (!m_bOffscreenHandlesAllocated)
	{
		m_offscreenRTVHandle = allocator->AllocateRTV();
		m_offscreenDSVHandle = allocator->AllocateDSV();
		m_offscreenSRVHandle = m_descriptorAllocator->AllocateStaticSlot();
		m_bOffscreenHandlesAllocated = true;
	}
	
	// Offscreen RTV
	{
		D3D12_CLEAR_VALUE clear{
			.Format = m_backbufferFormat,
			.Color = { 0.0f, 0.0f, 0.2f, 1.0f }
		};
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_backbufferFormat,
			static_cast<UINT64>(renderWidth),
			static_cast<UINT64>(renderHeight),
			1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clear,
			IID_PPV_ARGS(m_offscreenResource.ReleaseAndGetAddressOf()))
		);
		NAME_D3D12_OBJECT(m_offscreenResource);
	}

	// RTV & SRV 생성
	auto offscreenRTV = m_descriptorAllocator->GetRTVCpu(m_offscreenRTVHandle);
	m_device->CreateRenderTargetView(m_offscreenResource.Get(), nullptr, offscreenRTV);
	
	auto srvCPU = m_descriptorAllocator->GetStaticCpu(m_offscreenSRVHandle);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format = m_backbufferFormat,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = D3D12_TEX2D_SRV{
			.MostDetailedMip = 0,
			.MipLevels = 1
		}
	};
	
	m_device->CreateShaderResourceView(m_offscreenResource.Get(), &srvDesc, srvCPU);

	// Offscreen DSV
	{
		D3D12_CLEAR_VALUE clear{
		.Format = m_depthFormat,
		.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{
			.Depth = 1.0f,
			.Stencil = 0
		}
		};
		CD3DX12_RESOURCE_DESC dsDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_depthFormat,
			static_cast<UINT64>(renderWidth),
			static_cast<UINT64>(renderHeight),
			1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clear,
			IID_PPV_ARGS(m_offscreenDepth.ReleaseAndGetAddressOf()))
		);
		NAME_D3D12_OBJECT(m_offscreenDepth);
	}
	auto offscreenDSV = m_descriptorAllocator->GetDSVCpu(m_offscreenDSVHandle);
	m_device->CreateDepthStencilView(m_offscreenDepth.Get(), nullptr, offscreenDSV);

	// 변경된 타겟을 RenderSystem에 주입
	m_renderSystem->SetOutputTarget(m_offscreenResource.Get(), offscreenRTV, offscreenDSV);
	m_renderSystem->SetViewport(0.0f, 0.0f, renderWidth, renderHeight);
	m_renderSystem->CreateHitProxyTarget(static_cast<uint32_t>(renderWidth), static_cast<uint32_t>(renderHeight));
}
