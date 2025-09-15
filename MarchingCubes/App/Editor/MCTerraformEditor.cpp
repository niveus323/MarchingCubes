#include "pch.h"
#include "MCTerraformEditor.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Math/PhysicsHelper.h"
#include "App/Editor/Handles/VertexSelector.h"
#include <algorithm>
#include <typeinfo>
#include <iostream>

MCTerraformEditor::MCTerraformEditor(UINT width, UINT height, std::wstring name)
	: DXAppBase(width, height, name), 
	m_frameIndex(0), 
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 
	m_scissorRect(0,0,static_cast<LONG>(width), static_cast<LONG>(height)), 
	m_fenceValues{},
	m_rtvDescriptorSize(0),
	m_camera(nullptr)
{
}

MCTerraformEditor::~MCTerraformEditor()
{
	
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
	m_inputState.Update();

	if (m_inputState.IsPressed(ActionKey::Escape))
	{
		PostQuitMessage(0);
		return;
	}

	// TODO : 토글 키 개선 (2025-07-28 Author : DHLee)
	static bool prevToggleState = false;
	bool currentToggleState = m_inputState.IsPressed(ActionKey::ToggleDebugView) || m_inputState.IsPressed(ActionKey::ToggleWireFrame);
	if (currentToggleState && !prevToggleState)
	{
		if (m_inputState.IsPressed(ActionKey::ToggleDebugView))
		{
			m_debugViewEnabled = !m_debugViewEnabled;
			OutputDebugString(m_debugViewEnabled ? L"ID Render Target ON\n" : L"ID Render Target OFF\n");
		}
		else if (m_inputState.IsPressed(ActionKey::ToggleWireFrame))
		{
			// Rasterize State WireFrame으로 변경
			m_pipelineStates[PipelineMode::Filled].Swap(m_wireFramePSO);
		}
		
	}
	prevToggleState = currentToggleState;

	if (!m_uiRenderer->IsCapturingUI())
	{
		m_camera->Move(m_inputState, deltaTime);
		if (m_inputState.m_rightBtnState == MouseButtonState::Pressed)
		{
			m_camera->Rotate(m_inputState.m_mouseDeltaX, m_inputState.m_mouseDeltaY);
		}

		// 마우스 좌 버튼 (Terraform)
		const bool terraformHeld = m_inputState.m_leftBtnState == MouseButtonState::Pressed;

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
				BrushRequest req_brush{};
				req_brush.hitpos = hitPosLS;
				req_brush.radius = m_brushRadius;
				req_brush.weight = m_brushStrength * (m_inputState.IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f);
				req_brush.deltaTime = deltaTime;
				req_brush.isoValue = m_mcIso;
				m_terrain->requestBrush(req_brush);
			}
		}
	}

	// UI
	if (m_gridRenewRequested)
	{
		GridDesc gridDesc;
		gridDesc.cells = { (UINT)m_gridSize[0], (UINT)m_gridSize[1], (UINT)m_gridSize[2] };
		gridDesc.cellsize = m_cellSize;
		gridDesc.origin = m_gridOrigin;
		m_terrain->setGridDesc(m_device.Get(), gridDesc);
		m_gridRenewRequested = false;
	}

	m_camera->UpdateViewMatrix();
	m_camera->UpdateConstantBuffer();
	m_lightManager->Update();
}

void MCTerraformEditor::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

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
		ImGui::Text("Position : %.1f, %.1f, %.1f",m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z);
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
		ImGui::DragFloat("##Brush Radius", &m_brushRadius, 0.2f, 0.1f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

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

		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHawrdwardAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}
	NAME_D3D12_OBJECT(m_device);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	NAME_D3D12_OBJECT(m_commandQueue);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), Win32Application::GetHwnd(), &swapChainDesc, nullptr, nullptr, &swapChain));

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
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		NAME_D3D12_OBJECT(m_fence);
		for (UINT i = 0; i < FrameCount; ++i) m_fenceValues[i] = 1;
	}
	

	m_uploadContext.Initailize(m_device.Get());
}

void MCTerraformEditor::LoadAssets()
{
	CreatePipelineStates();

	// Create Bundle Recorder.
	m_bundleRecorder = std::make_unique<BundleRecorder>(m_device.Get(), m_rootSignature.Get(), m_pipelineStates, 2);

	// Create CommandList.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);

	// Close CommandList
	ThrowIfFailed(m_commandList->Close());

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

	// Set Default Instance MCGenerator
	//MarchingCubesGenerator::SetInstance(std::make_unique<OriginalMC>());
	
	// TerrainSystem 초기화
	{
		GridDesc gridDesc{};
		gridDesc.cells = { 100, 100, 100 };
		gridDesc.cellsize = 1.0f;
		gridDesc.origin = { m_gridOrigin.x - 50.0f, m_gridOrigin.y - 50.0f, m_gridOrigin.z - 50.0f };
		auto initialSphereField = MakeSphereGrid(100, 1.0f, 25.0f, m_gridOrigin);
		m_terrain = std::make_unique<TerrainSystem>(m_device.Get(), initialSphereField, gridDesc, TerrainMode::GPU_ORIGINAL);
		RemeshRequest req(m_mcIso);
		m_terrain->requestRemesh(req);

		TerrainChunkRenderer* terrainMesh = m_terrain->GetRenderer();
		terrainMesh->SetMaterial(m_defaultMat);

		DynamicRenderItem terrainMeshItem;
		terrainMeshItem.object = terrainMesh;
		m_dynamicRenderItems[PipelineMode::Filled].push_back(terrainMeshItem);
	}

	// CreateUploadBuffer Cube Mesh
	{
		/*MeshData cubeData = MeshGenerator::GenerateCubeGrid(1, 1, 1);
		m_gridMesh.SetMaterial(m_defaultMat);
		DynamicRenderItem cubeMeshItem = m_uploadContext.UploadDynamicMesh(m_gridMesh, cubeData);
		m_dynamicRenderItems[PipelineMode::Line].push_back(cubeMeshItem);
		*/
		/*for (int i = 0; i < cubeData.vertices.size(); ++i)
		{
			auto& vertex = cubeData.vertices[i];
			auto vertexSelector = std::make_unique<VertexSelector>(m_device.Get(), m_uploadContext, vertex.pos, i+1);
			m_dynamicRenderItems[PipelineMode::Filled].push_back(vertexSelector->GetRenderItem());
			m_pickables.push_back(std::move(vertexSelector));
		}*/
		
		// TODO : MC33 라이브러리로 수정하였으므로 큐브 정점 순서 상관없어짐.
		static constexpr int ofs[8][3] = {
			{0,0,0},{1,0,0},{1,1,0},{0,1,0},
			{0,0,1},{1,0,1},{1,1,1},{0,1,1}
		};
		auto Flat = [](int x, int y, int z, int VY, int VZ) { return x * (VY * VZ) + y * VZ + z; };

		std::array<UINT, 8> corner;
		for (int v = 0; v < 8; ++v)
		{
			corner[v] = Flat(ofs[v][0], ofs[v][1], ofs[v][2], 2, 2);
		}
		m_cells.push_back(corner);
		 
		//MeshData reservedMesh;
		//reservedMesh.vertices.reserve(15);
		//reservedMesh.indices.reserve(15);
		//m_generatedMesh.SetMaterial(m_defaultMat);
		//DynamicRenderItem generatedMeshitem = m_uploadContext.UploadDynamicMesh(m_generatedMesh, reservedMesh);
		//m_dynamicRenderItems[PipelineMode::Filled].push_back(generatedMeshitem);
	}
}


void MCTerraformEditor::CreatePipelineStates()
{
	ComPtr<ID3DBlob> vertexShader, pixelShader;
	ComPtr<ID3DBlob> linePS;
	ComPtr<ID3DBlob> pickIDPS;

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	
	// TODO : 너무 느려서 shader cache 이용하는 방식으로 수정. D3DReadFileToBlob.
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"MainPS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"LineShaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &linePS, nullptr));

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
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
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
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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
	}
}

void MCTerraformEditor::PopulateCommandList()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	// 교체되는 리소스를 Sink에 수집.
	m_toDeletesContainer.clear();
	m_uploadContext.SetDeletionSink(&m_toDeletesContainer);

	m_uploadContext.Execute(m_commandList.Get());

	if (m_terrain)
	{
		// 이전 프레임에서 작업한 결과를 우선 적용
		m_terrain->tryFetch(m_device.Get(), m_commandList.Get());

		m_terrain->encode(m_commandList.Get());
		m_terrain->drainKeepAlive(m_toDeletesContainer);
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
	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get()};
	m_commandList->SetDescriptorHeaps(1, ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(4, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

	// 렌더 타겟 생성됨, Back Buffer를 RenderTarget 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// RTV Clear 명령 추가.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// PSO 별로 Draw Command 실행
	for (PipelineMode mode : { PipelineMode::Filled, PipelineMode::Line })
	{
		m_commandList->SetPipelineState(m_pipelineStates[mode].Get());

		// 정적 object 렌더링
		for (auto item : m_staticRenderItems[mode])
		{
			// Bundle에 있던 Command 실행.
			m_commandList->ExecuteBundle(item.bundle);
		}

		// Bind Camera Constant Buffer
		m_camera->BindConstantBuffer(m_commandList.Get(), 0);
		// Bind Lights
		m_lightManager->BindConstant(m_commandList.Get());

		// 동적 object 렌더링
		for (auto item : m_dynamicRenderItems[mode])
		{
			item.object->Draw(m_commandList.Get());
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
	assert(m_fence);
#endif // _DEBUG

	// Signal 명령을 queue에 추가.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// fence가 완료되는 시점까지 대기(시점 동기화).
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));

	// 이벤트 던져놓고 대기.
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// 현재 frame에 대한 fence 값 ++.
	m_fenceValues[m_frameIndex]++;
}

void MCTerraformEditor::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	if (!m_toDeletesContainer.empty())
	{
		PendingDeleteItem pd;
		pd.fenceValue = currentFenceValue;
		pd.resources.swap(m_toDeletesContainer);
		m_pendingDeletes.emplace_back(std::move(pd));
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// 삭제 요청 이행.
	const UINT64 completed = m_fence->GetCompletedValue();
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

std::shared_ptr<_GRD> MCTerraformEditor::MakeSphereGrid(int N, float cellSize, float radius, XMFLOAT3 center = { 0.0f, 0.0f, 0.0f })
{
	const float half = 0.5f * (float)N;
	XMFLOAT3 origin = { center.x - half * cellSize, center.y - half * cellSize, center.z - half * cellSize };

	auto gridData = new _GRD{};
	gridData->N[0] = N;			 gridData->N[1] = N;		  gridData->N[2] = N;           // 셀 개수
	gridData->d[0] = cellSize;   gridData->d[1] = cellSize;   gridData->d[2] = cellSize;    // 셀 크기
	gridData->r0[0] = origin.x;  gridData->r0[1] = origin.y;  gridData->r0[2] = origin.z;   // [-0.5, 0.5] 범위가 될 수 있도록 grid의 0번은 (-0.5, -0.5, -0.5)
	gridData->nonortho = 0;
	gridData->periodic = 0;

	// 샘플 수 = (N+1)^3
	const int SX = N + 1, SY = N + 1, SZ = N + 1;

	// 채우기: F = radius - |p - center|
	gridData->F = new float** [SZ];
	for (int z = 0; z < SZ; ++z)
	{
		gridData->F[z] = new float* [SY];
		float dz = (z - half) * cellSize;
		for (int y = 0; y < SY; ++y) 
		{
			gridData->F[z][y] = new float[SX];
			float dy = (y - half) * cellSize;
			for (int x = 0; x < SX; ++x) 
			{
				float dx = (x - half) * cellSize;
				const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
				gridData->F[z][y][x] = radius - dist; // 내부>0, 표면=0, 외부<0
			}
		}
	}
	//m_mcIso = 0.5f;
	m_mcIso = 0.0f;

	return std::shared_ptr<_GRD>(gridData);
}