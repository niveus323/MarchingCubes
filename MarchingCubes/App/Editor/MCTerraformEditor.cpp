#include "pch.h"
#include "MCTerraformEditor.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Math/PhysicsHelper.h"
#include <algorithm>
#include <typeinfo>
#include <iostream>

int sDebugFrame = 0;

MCTerraformEditor::MCTerraformEditor(UINT width, UINT height, std::wstring name)
	: DXAppBase(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_fenceValues{},
	m_rtvDescriptorSize(0),
	m_camera(nullptr),
	m_fenceEvent(0)
{
}

MCTerraformEditor::~MCTerraformEditor()
{
#if PIX_DEBUGMODE
	if (PIXGetCaptureState() == PIX_CAPTURE_GPU)
	{
		PIXEndCapture(FALSE);
	}
#endif
}

void MCTerraformEditor::OnInit()
{
	LoadPipeline();
	LoadAssets();
	OnInitUI();
}

void MCTerraformEditor::OnInitUI()
{
	// SRV Heap 초기화
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> srvHeap;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));
	NAME_D3D12_OBJECT(srvHeap);

	std::unique_ptr<ImGUIRenderer> imguiRenderer = std::make_unique<ImGUIRenderer>();

	UIRenderInitContext initContext = {};
	initContext.device = m_device.Get();

	ImGUIInitOptions initOptions = {};
	initOptions.nums_of_frame = FrameCount;
	initOptions.srvHeap = srvHeap.Get();
	initOptions.cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
	initOptions.gpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

	initContext.userData = std::any(initOptions);

	if (!imguiRenderer->Initialize(initContext))
	{
		MessageBox(Win32Application::GetHwnd(), m_uiRenderer->GetLastErrorMsg().c_str(), L"UI Initialization Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(-1);
		return;
	}

	m_uiRenderer = std::move(imguiRenderer);
}

void MCTerraformEditor::OnUpdate(float deltaTime)
{
	if (m_inputState.IsPressed(ActionKey::Escape))
	{
		PostQuitMessage(0);
		return;
	}

	if (m_inputState.GetKeyState(ActionKey::ToggleDebugView) == ActionKeyState::JustPressed)
	{
		m_debugViewEnabled = !m_debugViewEnabled;
	}
	else if (m_inputState.GetKeyState(ActionKey::ToggleWireFrame) == ActionKeyState::JustPressed)
	{
		// Rasterize State WireFrame으로 변경
		m_pipelineStates[PipelineMode::Filled].Swap(m_wireFramePSO);
	}
	else if (m_inputState.GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustPressed)
	{
		m_debugNormalEnabled = !m_debugNormalEnabled;
	}


	if (!m_uiRenderer->IsCapturingUI())
	{
		m_camera->Move(m_inputState, deltaTime);
		if (m_inputState.m_rightBtnState == ActionKeyState::Pressed)
		{
			m_camera->Rotate(m_inputState.m_mouseDeltaX, m_inputState.m_mouseDeltaY);
		}

		// 마우스 좌 버튼 (Terraform)
		const bool terraformHeld = m_inputState.m_leftBtnState == ActionKeyState::Pressed;

		if (terraformHeld)
		{
			TerrainChunkRenderer* terrainRenderer = m_terrain->GetRenderer();

			XMVECTOR rayOrigin, rayDir;
			PhysicsUtil::MakeRay(static_cast<float>(m_inputState.m_mouseX), static_cast<float>(m_inputState.m_mouseY), *m_camera.get(), rayOrigin, rayDir);

			XMVECTOR rayOriginls = XMVector3TransformCoord(rayOrigin, terrainRenderer->GetWorldInvMatrix());
			// normalized된 방향 벡터는 회전 행렬에 대한 역변환만 고려하면되지만 편의를 위해 WorldInvMatrix를 사용
			XMVECTOR rayDirls = XMVector3Normalize(XMVector3TransformCoord(rayDir, terrainRenderer->GetWorldInvMatrix()));

			// RayCast로 terrainMesh를 피킹중인지 확인
			XMFLOAT3 hitPosLS;
			if (PhysicsUtil::IsHit(rayOriginls, rayDirls, terrainRenderer->GetMeshData(), terrainRenderer->GetBoundingBox(), hitPosLS))
			{
#if PIX_DEBUGMODE
				if (m_debugViewEnabled && sDebugFrame == 0 && PIXGetCaptureState() == 0)
				{
					std::cout << "BeginCapture" << std::endl;
					PIXCaptureParameters param = {};
					/*param.TimingCaptureParameters.FileName = L"Brush.wpix";
					param.TimingCaptureParameters.CaptureCallstacks = true;
					param.TimingCaptureParameters.CaptureCpuSamples = true;
					param.TimingCaptureParameters.CaptureGpuTiming = true;
					param.TimingCaptureParameters.CpuSamplesPerSecond = 4000;*/
					param.GpuCaptureParameters.FileName = L"Brush.wpix";
					PIXBeginCapture(PIX_CAPTURE_GPU, &param);
				}
#endif
#ifdef _DEBUG
				// Hit가 발생한 위치에 원 세팅
				{
					XMVECTOR vHitposLS = XMLoadFloat3(&hitPosLS);
					XMVECTOR vHitposGS = XMVector3TransformCoord(vHitposLS, terrainRenderer->GetWorldMatrix());
					
					XMFLOAT3 pos;
					XMStoreFloat3(&pos, vHitposGS);

					m_debugBrush->SetPosition(pos);
					m_debugBrush->UpdateConstants();
				}
#endif // DEBUG
				BrushRequest req_brush{};
				req_brush.hitpos = hitPosLS;
				req_brush.radius = m_brushRadius;
				req_brush.weight = m_brushStrength * (m_inputState.IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f);
				req_brush.deltaTime = deltaTime;
				req_brush.isoValue = m_mcIso;
				m_terrain->requestBrush(req_brush);
			}
#ifdef _DEBUG
			//else
			//{
			//	m_debugBrush->SetColor({ 1.0f, 0.0f, 0.0f, 0.0f }); // 안보이게 세팅
			//}
#endif // _DEBUG
		}
	}

	// UI
	if (m_gridRenewRequested)
	{
		GridDesc gridDesc;
		gridDesc.cells = { (UINT)m_gridSize[0], (UINT)m_gridSize[1], (UINT)m_gridSize[2] };
		gridDesc.cellsize = static_cast<float>(m_cellSize);
		gridDesc.origin = m_gridOrigin;
		m_terrain->setGridDesc(m_device.Get(), gridDesc);
		m_gridRenewRequested = false;
	}

	m_camera->UpdateViewMatrix();
	m_camera->UpdateConstantBuffer();
	m_lightManager->Update();
	m_inputState.Update();

	m_terrain->tryFetch(m_device.Get(), m_swapChainFence.Get(), &m_toDeletesContainer);
}

void MCTerraformEditor::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_graphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(m_swapChain->Present(1, 0));

	MoveToNextFrame();
}

void MCTerraformEditor::OnRenderUI()
{
	// UI 렌더링 코드
	ImGui::Text("FPS : %.1f", ImGui::GetIO().Framerate);

	// Camera
	{
		ImGui::Begin("Camera & Light");
		ImGui::Text("Position : %.1f, %.1f, %.1f", m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z);
		ImGui::Text("MoveSpeed");
		ImGui::SliderFloat("##MoveSpeed", &m_camera->GetMoveSpeedPtr(), 1.0f, 25.0f);
		ImGui::Text("Light Direction");
		if (ImGui::DragFloat3("## Light Direction", m_lightDir.data(), 0.1f, -1.0f, 1.0f))
		{
			XMFLOAT3 newDir = { -m_lightDir[0], -m_lightDir[1], -m_lightDir[2] };
			m_lightManager->SetDirection(newDir);
		}
		ImGui::End();
	}

	// MarchingCubes
	{
		ImGui::Begin("Marching Cubes Options");

		ImGui::Text("Origin");
		ImGui::SliderFloat3("##Origin", &m_gridOrigin.x, -10.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

		ImGui::Text("Num Of Tiles");
		ImGui::InputInt3("##Num Of Tiles", m_gridSize.data()/*, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue*/);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			m_gridSize[0] = std::clamp(m_gridSize[0], 1, 16);
			m_gridSize[1] = std::clamp(m_gridSize[1], 1, 16);
			m_gridSize[2] = std::clamp(m_gridSize[2], 1, 16);

			m_gridRenewRequested = true;
		}

		ImGui::Text("Cell Size");
		if (ImGui::InputInt("##Num Of Tiles", &m_cellSize, 1, 4, ImGuiInputTextFlags_AlwaysOverwrite))
		{
			m_cellSize = std::clamp(m_cellSize, 1, 16);
			m_gridRenewRequested = true;
		}

		ImGui::Text("Brush Radius");
		ImGui::DragFloat("##Brush Radius", &m_brushRadius, 0.2f, 3.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			MeshData newBrushMeshData;
			MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
			m_uploadContext.UpdateMesh(m_swapChainFence.Get(), *m_debugBrush.get(), newBrushMeshData);
		}

		ImGui::Text("Brush Strength");
		ImGui::DragFloat("##Brush Strength", &m_brushStrength, 1.0f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

		ImGui::End();
	}

	m_uiRenderer->EndFrame(m_commandList.Get());
}

void MCTerraformEditor::OnDestroy()
{
	WaitForGpu();
	CloseHandle(m_fenceEvent);
}

void MCTerraformEditor::OnKeyDown(WPARAM key)
{
	m_inputState.OnKeyDown(key);
}

void MCTerraformEditor::OnKeyUp(WPARAM key)
{
	m_inputState.OnKeyUp(key);
}

void MCTerraformEditor::OnMouseMove(int xPos, int yPos, WPARAM buttonState)
{
	//TODO : Drag 구현
	m_inputState.OnMouseMove(xPos, yPos);
}

void MCTerraformEditor::OnMouseBtnDown(int x, int y, WPARAM button)
{
	//TODO : Drag 구현
	m_inputState.OnMouseDown(x, y, button);
}

void MCTerraformEditor::OnMouseBtnUp(int x, int y, WPARAM button)
{
	m_inputState.OnMouseUp(x, y, button);
}

void MCTerraformEditor::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

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
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_userWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHawrdwardAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
	}
	NAME_D3D12_OBJECT(m_device);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_graphicsQueue)));
	NAME_D3D12_OBJECT(m_graphicsQueue);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(m_graphicsQueue.Get(), Win32Application::GetHwnd(), &swapChainDesc, nullptr, nullptr, &swapChain));

	//FullScreen Support Setting
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	//Descriptor Heap 생성
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		NAME_D3D12_OBJECT(m_rtvHeap);

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
		NAME_D3D12_OBJECT(m_srvHeap);

		D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
		dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvDesc.NumDescriptors = 1;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap)));
		NAME_D3D12_OBJECT(m_dsvHeap);
	}

	//Frame Resource 생성
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);

			rtvHandle.Offset(1, m_rtvDescriptorSize);

			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
			NAME_D3D12_OBJECT_INDEXED(m_commandAllocators, n);
		}

	}

	// SRV (Env Texture) 생성
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		m_device->CreateShaderResourceView(nullptr, &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// DSV 생성
	{ // TODO : 윈도우 리사이즈 시 재생성하도록 대응하기.

		D3D12_CLEAR_VALUE clear{};
		clear.Format = DXGI_FORMAT_D32_FLOAT;
		clear.DepthStencil.Depth = 1.0f;
		clear.DepthStencil.Stencil = 0;

		CD3DX12_RESOURCE_DESC dsDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clear,
			IID_PPV_ARGS(&m_depthStencil))
		);

		m_device->CreateDepthStencilView(m_depthStencil.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// fence 생성
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_swapChainFence)));
		NAME_D3D12_OBJECT(m_swapChainFence);
		for (UINT i = 0; i < FrameCount; ++i) m_fenceValues[i] = 1;

		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_uploadFence)));
		NAME_D3D12_OBJECT(m_uploadFence);
	}


	m_uploadContext.Initailize(m_device.Get());

	const UINT64 uploadRingSize = 16ull * 1024 * 1024;
	m_uploadRing.Initialize(m_device.Get(), uploadRingSize);
	g_uploadRing = &m_uploadRing;
}

void MCTerraformEditor::LoadAssets()
{
	CreatePipelineStates();

	// Create Bundle Recorder.
	m_bundleRecorder = std::make_unique<BundleRecorder>(m_device.Get(), m_rootSignature.Get(), m_pipelineStates, 2);

	// Create CommandList.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);

	// Initaizlie Camera
	{
		m_camera = std::make_unique<Camera>(static_cast<float>(m_width), static_cast<float>(m_height));
		m_camera->CreateConstantBuffer(m_device.Get());
	}

	// Initialize Lights
	{
		const UINT s_lightCount = 1;
		m_lightManager = std::make_unique<LightManager>(m_device.Get(), s_lightCount, 3);
		m_lightManager->AddDirectional({ -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
		m_lightDir = { 1.0f, -1.0f, 0.0f };
	}

	// Initialize Materials
	{
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
	}

	// TerrainSystem 초기화
	{
		GridDesc gridDesc{};
		auto initialSphereField = MakeSphereGrid(100U, 1.0f, 25.0f, m_gridOrigin, gridDesc);
		m_terrain = std::make_unique<TerrainSystem>(m_device.Get(), initialSphereField, gridDesc, TerrainMode::CPU_MC33);
		RemeshRequest req(m_mcIso);
		m_terrain->requestRemesh(req);

		TerrainChunkRenderer* terrainMesh = m_terrain->GetRenderer();
		terrainMesh->SetMaterial(m_defaultMat);

		DynamicRenderItem terrainMeshItem;
		terrainMeshItem.object = terrainMesh;
		m_dynamicRenderItems[PipelineMode::Filled].push_back(terrainMeshItem);
	}


#ifdef _DEBUG
	/*
	// Debug Terrain Cell
	{
		std::unique_ptr<Mesh> debugCellMesh = std::make_unique<Mesh>();
		MeshData debugcelldata;
		m_terrain->MakeDebugCell(debugcelldata);
		m_uploadContext.UploadStaticMesh(m_swapChainFence.Get(), m_commandList.Get(), *debugCellMesh.get(), debugcelldata);
		m_StaticObjects.push_back(std::move(debugCellMesh));
		StaticRenderItem debugCellMeshItem = m_bundleRecorder->CreateBundleFor(m_StaticObjects, PipelineMode::Line);
		m_staticRenderItems[PipelineMode::Line].push_back(debugCellMeshItem);
	}
	*/
	
	// Debug Brush
	{
		m_debugBrush = std::make_unique<Mesh>();
		MeshData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		auto item = m_uploadContext.UploadDynamicMesh(*m_debugBrush.get(), m_swapChainFence.Get(), debugBrushData);
		m_dynamicRenderItems[PipelineMode::Wire].push_back(item);
	}	

#endif // _DEBUG


	// Close CommandList
	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_graphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();

}


void MCTerraformEditor::CreatePipelineStates()
{
	ComPtr<ID3DBlob> mainVS, mainPS;
	ComPtr<ID3DBlob> linePS;
	ComPtr<ID3DBlob> pickIDPS;
	ComPtr<ID3DBlob> normalGS;

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	// TODO : 너무 느려서 shader cache 이용하는 방식으로 수정. D3DReadFileToBlob.
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &mainVS, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"MainPS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &mainPS, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"LineShaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &linePS, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"NormalGS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "GSMain", "gs_5_0", compileFlags, 0, &normalGS, nullptr));

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
		CD3DX12_DESCRIPTOR_RANGE1 range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
		rootParams[4].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		// Static Sampler 등록 ( 별도의 샘플러가 필요할 경우 Descriptor Table에 포함할 것.)
		D3D12_STATIC_SAMPLER_DESC samplerDesc{};
		samplerDesc.ShaderRegister = 0; // s0
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, nullptr));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
	}

	// Main PBR Shader
	{
		// Create Graphics Pipeline State Object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(mainVS.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(mainPS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ComPtr <ID3D12PipelineState> pso;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		m_pipelineStates[PipelineMode::Filled] = pso;
		NAME_D3D12_OBJECT_ALIAS_INDEXED(m_pipelineStates, PipelineMode::Filled, ToLPCWSTR(PipelineMode::Filled));
	}

	// Line PSO
	{
		// Create Graphics Pipeline State Object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(mainVS.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(linePS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ComPtr <ID3D12PipelineState> pso;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		m_pipelineStates[PipelineMode::Line] = pso;
		NAME_D3D12_OBJECT_ALIAS_INDEXED(m_pipelineStates, PipelineMode::Line, ToLPCWSTR(PipelineMode::Line));
	}

	//WireFrame PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(mainVS.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(linePS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ComPtr <ID3D12PipelineState> pso;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		m_wireFramePSO = pso;
		NAME_D3D12_OBJECT(m_wireFramePSO);
		m_pipelineStates[PipelineMode::Wire] = pso;
		NAME_D3D12_OBJECT_ALIAS_INDEXED(m_pipelineStates, PipelineMode::Wire, ToLPCWSTR(PipelineMode::Wire));
	}

	//DebugNormal PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(mainVS.Get());
		psoDesc.GS = CD3DX12_SHADER_BYTECODE(normalGS.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(linePS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ComPtr <ID3D12PipelineState> pso;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		m_debugNormalPSO = pso;
		NAME_D3D12_OBJECT(m_debugNormalPSO);
	}
}

void MCTerraformEditor::PopulateCommandList()
{
	if (m_swapChainFence)
	{
		m_uploadRing.ReclaimCompleted(m_swapChainFence->GetCompletedValue());
	}

	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	// 교체되는 리소스를 Sink에 수집.
	m_toDeletesContainer.clear();
	m_uploadContext.SetDeletionSink(&m_toDeletesContainer);

	m_uploadContext.Execute(m_commandList.Get());

	if (m_terrain)
	{
		m_terrain->UploadRendererData(m_device.Get(), m_commandList.Get(), m_allocationsThisSubmit);
	}

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Bind Camera Constant Buffer
	m_camera->BindConstantBuffer(m_commandList.Get(), 0);

	// Bind Lights
	m_lightManager->BindConstant(m_commandList.Get());

	// Descriptor Heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(1, ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(4, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

	// 렌더 타겟 생성됨, Back Buffer를 RenderTarget 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// RTV Clear 명령 추가.
	const float clearColor[] = { 0.0f, 0.0f, 0.2f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// PSO 별로 Draw Command 실행
	for (int i = 0; i < static_cast<int>(PipelineMode::Count); ++i)
	{
		PipelineMode mode = static_cast<PipelineMode>(i);
		m_commandList->SetPipelineState(m_pipelineStates[mode].Get());

		// Bind Camera Constant Buffer
		m_camera->BindConstantBuffer(m_commandList.Get(), 0);
		// Bind Lights
		m_lightManager->BindConstant(m_commandList.Get());

		// 정적 object 렌더링
		for (auto item : m_staticRenderItems[mode])
		{
			// Bundle에 있던 Command 실행.
			m_commandList->ExecuteBundle(item.bundle);
		}

		// 동적 object 렌더링
		for (auto item : m_dynamicRenderItems[mode])
		{
			item.object->Draw(m_commandList.Get());
		}
	}

	if (m_debugNormalEnabled)
	{
		m_commandList->SetPipelineState(m_debugNormalPSO.Get());
		for (PipelineMode mode : { PipelineMode::Filled, PipelineMode::Line })
		{
			m_camera->BindConstantBuffer(m_commandList.Get(), 0);
			m_lightManager->BindConstant(m_commandList.Get());

			for (auto item : m_staticRenderItems[mode])
			{
				// Bundle에 있던 Command 실행.
				m_commandList->ExecuteBundle(item.bundle);
			}

			for (auto item : m_dynamicRenderItems[mode])
			{
				item.object->Draw(m_commandList.Get());
			}
		}

	}

	OnRenderUI();

	// 렌더링 끝났음. Present 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void MCTerraformEditor::WaitForGpu()
{
#ifdef _DEBUG
	assert(m_swapChainFence);
#endif // _DEBUG

	// Signal 명령을 queue에 추가.
	ThrowIfFailed(m_graphicsQueue->Signal(m_swapChainFence.Get(), m_fenceValues[m_frameIndex]));

	// fence가 완료되는 시점까지 대기(시점 동기화).
	ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));

	// 이벤트 던져놓고 대기.
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// 현재 frame에 대한 fence 값 ++.
	m_fenceValues[m_frameIndex]++;
}

void MCTerraformEditor::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_graphicsQueue->Signal(m_swapChainFence.Get(), currentFenceValue));

	// 이번 프레임에 할당했던 요소들에 대해 완료 대기를 걸어둔다 
	for (auto& a : m_allocationsThisSubmit) {
		m_uploadRing.TrackAllocation(a.first, a.second, currentFenceValue);
	}
	m_allocationsThisSubmit.clear();

	if (!m_toDeletesContainer.empty())
	{
		PendingDeleteItem pd;
		pd.fenceValue = currentFenceValue;
		pd.resources.swap(m_toDeletesContainer);
		m_pendingDeletes.emplace_back(std::move(pd));
	}

	// 체인 스왑
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// 체인 스왑 전에 완료하지 못한 작업은 대기
	if (m_swapChainFence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_swapChainFence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

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

	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

std::shared_ptr<SdfField<float>> MCTerraformEditor::MakeSphereGrid(unsigned int N, float cellSize, float radius, XMFLOAT3 center, GridDesc& OutGridDesc)
{
	const float half = 0.5f * (float)N;
	XMFLOAT3 origin = { center.x - half * cellSize, center.y - half * cellSize, center.z - half * cellSize };

	OutGridDesc.cells = { N, N, N };
	OutGridDesc.cellsize = cellSize;
	OutGridDesc.origin = origin;

	// 샘플 수 = (N+1)^3
	const int SX = N + 1, SY = N + 1, SZ = N + 1;

	// 채우기: F = radius - |p - center|
	auto gridData = new SdfField<float>(SX, SY, SZ);
	for (int z = 0; z < SZ; ++z)
	{
		float dz = (z - half) * cellSize;
		for (int y = 0; y < SY; ++y)
		{
			float dy = (y - half) * cellSize;
			for (int x = 0; x < SX; ++x)
			{
				// 내부>0, 표면=0, 외부<0
				float dx = (x - half) * cellSize;
				const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
				gridData->at(x, y, z) = std::clamp((radius - dist) / N, -1.0f, 1.0f);
			}
		}
	}
	m_mcIso = 0.0f;

	return std::shared_ptr<SdfField<float>>(gridData);
}