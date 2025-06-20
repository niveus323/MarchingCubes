#include "pch.h"
#include "D3D12HelloWindow.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/UI/ImGUIRenderer.h"
#include <algorithm>

//디버깅용
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

D3D12HelloWindow::D3D12HelloWindow(UINT width, UINT height, std::wstring name)
	: DXAppBase(width, height, name), 
	m_frameIndex(0), 
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 
	m_scissorRect(0,0,static_cast<LONG>(width), static_cast<LONG>(height)), 
	m_fenceValues{},
	m_rtvDescriptorSize(0),
	m_camera(nullptr)
{
}

void D3D12HelloWindow::OnInit()
{
	LoadPipeline();
	LoadAssets();
	OnInitUI();
}

void D3D12HelloWindow::OnInitUI()
{
	// SRV Heap 초기화
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));

	std::unique_ptr<ImGUIRenderer> imguiRenderer = std::make_unique<ImGUIRenderer>();

	UIRenderInitContext initContext = {};
	initContext.device = m_device.Get();

	ImGUIInitOptions initOptions = {};
	initOptions.nums_of_frame = FrameCount;
	initOptions.srvHeap = m_srvHeap.Get();
	initOptions.cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	initOptions.gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

	initContext.userData = std::any(initOptions);
	
	if (!imguiRenderer->Initialize(initContext))
	{
		MessageBox(Win32Application::GetHwnd(), m_uiRenderer->GetLastErrorMsg().c_str(), L"UI Initialization Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(-1);
		return;
	}

	m_uiRenderer = std::move(imguiRenderer);
}

void D3D12HelloWindow::OnUpdate(float deltaTime)
{
	if (m_inputState.IsPressed(ActionKey::Escape))
	{
		PostQuitMessage(0);
		return;
	}

	static bool prevToggleState = false;
	bool currentToggleState = m_inputState.IsPressed(ActionKey::ToggleDebugView);
	if (currentToggleState && !prevToggleState)
	{
		m_debugViewEnabled = !m_debugViewEnabled;
		OutputDebugString(m_debugViewEnabled ? L"ID Render Target ON\n" : L"ID Render Target OFF\n");
	}
	prevToggleState = currentToggleState;

	m_camera->Move(m_inputState, deltaTime);
	if (m_inputState.m_rightBtnState == MouseButtonState::Pressed)
	{
		m_camera->Rotate(m_inputState.m_mouseDeltaX, m_inputState.m_mouseDeltaY);
	}
	m_camera->UpdateViewMatrix();
	m_camera->UpdateConstantBuffer();

	if (m_inputState.m_leftBtnState == MouseButtonState::JustReleased)
	{
		m_pendingMouseX = m_inputState.m_mouseX;
		m_pendingMouseY = m_inputState.m_mouseY;
		m_pendingPick = true;
	}

	m_cubeMesh.Update();

	for (auto& obj : m_pickables)
	{
		obj->GetMesh()->Update();
	}

	m_inputState.Update();
}

void D3D12HelloWindow::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();

	// GPU 명령 제출 후, 읽기 가능한 시점
	if (m_pendingPick)
	{
		uint32_t id = ReadPickedID();

		wchar_t buf[128];
		swprintf_s(buf, L"Picked id: %d\n", id);
		OutputDebugString(buf);

		HandlePickedObject(id);
	}


	ThrowIfFailed(m_swapChain->Present(1, 0));

	MoveToNextFrame();
}

void D3D12HelloWindow::OnRenderUI()
{
	m_uiRenderer->BeginFrame();

	// UI 렌더링 코드
	ImGui::Text("FPS : %.1f", ImGui::GetIO().Framerate);

	m_uiRenderer->EndFrame(m_commandList.Get());
}

void D3D12HelloWindow::OnDestroy()
{
	WaitForGpu();

	CloseHandle(m_fenceEvent);
}

void D3D12HelloWindow::OnKeyDown(WPARAM key)
{
	m_inputState.OnKeyDown(key);
}

void D3D12HelloWindow::OnKeyUp(WPARAM key)
{
	m_inputState.OnKeyUp(key);
}

void D3D12HelloWindow::OnMouseMove(int xPos, int yPos, WPARAM buttonState)
{
	//TODO : Drag 구현
	m_inputState.OnMouseMove(xPos, yPos);
}

void D3D12HelloWindow::OnMouseBtnDown(int x, int y, WPARAM button)
{
	//TODO : Drag 구현
	m_inputState.OnMouseDown(x, y, button);
}

void D3D12HelloWindow::OnMouseBtnUp(int x, int y, WPARAM button)
{
	m_inputState.OnMouseUp(x, y, button);
}

void D3D12HelloWindow::LoadPipeline()
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

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

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
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	//Frame Resource 생성
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}

		// NOTE : 정적인 삼각형 그리기 명령은 하나의 Bundle을 재사용하기 때문에 Allocatior 수 만큼 만들지 않음. 만약 프레임마다 변경이 일어나야한다면 프레임 수 만큼 만들것.
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_bundleAllocator)));
	}	

	CreateIDRenderTarget();

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
}

void D3D12HelloWindow::LoadAssets()
{
	//Create Main Root Signature.
	{
		// Define Root Parameter
		CD3DX12_ROOT_PARAMETER rootParams[2];
		ZeroMemory(rootParams, sizeof(rootParams));

		rootParams[0].InitAsConstantBufferView(0); // CameraBuffer
		rootParams[1].InitAsConstantBufferView(1); // ObjectBuffer

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	CreatePipelineStates();

	// Create CommandList.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	// Close CommandList
	ThrowIfFailed(m_commandList->Close());

	// Initaizlie Camera
	{
		m_camera = new Camera(m_aspectRatio);
		m_camera->CreateConstantBuffer(m_device.Get());
	}

	// Initialize Cube Mesh
	{
		MeshData CubeData;
		//Front
		CubeData.vertices = {
			{ {-1.0f, -1.0f, -1.0f}, { 1, 0, 0, 1 } },
			{ {-1.0f,  1.0f, -1.0f}, { 0, 1, 0, 1 } },
			{ { 1.0f,  1.0f, -1.0f}, { 0, 0, 1, 1 } },
			{ { 1.0f, -1.0f, -1.0f}, { 1, 1, 0, 1 } },
			{ {-1.0f, -1.0f,  1.0f}, { 1, 0, 1, 1 } },
			{ {-1.0f,  1.0f,  1.0f}, { 0, 1, 1, 1 } },
			{ { 1.0f,  1.0f,  1.0f}, { 1, 1, 1, 1 } },
			{ { 1.0f, -1.0f,  1.0f}, { 0, 0, 0, 1 } }
		};
		CubeData.indices = {
			0, 1, 1, 2, 2, 3, 3, 0,
			4, 5, 5, 6, 6, 7, 7, 4,
			0, 4, 1, 5, 2, 6, 3, 7
		};
		CubeData.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		
		UploadContext meshUploader(m_device.Get());
		meshUploader.Begin();

		meshUploader.UploadMesh(m_cubeMesh, CubeData);
		m_pickables.push_back(std::make_unique<VertexSelector>(m_device.Get(), meshUploader, XMFLOAT3{ -1,-1,-1 }));
		m_pickables.push_back(std::make_unique<VertexSelector>(m_device.Get(), meshUploader, XMFLOAT3{1,1,1}));

		meshUploader.End(m_commandQueue.Get());
		
		BundleRecorder bundleRecorderLine(m_device.Get(), m_pipelineStates[PipelineMode::Line].Get(), m_rootSignature.Get());
		BundleRecorder bundleRecorderFilled(m_device.Get(), m_pipelineStates[PipelineMode::Filled].Get(), m_rootSignature.Get());
		m_bundles[PipelineMode::Line].push_back(bundleRecorderLine.CreateBundleFor(&m_cubeMesh.m_buffer, m_bundleAllocator.Get()));
		for (const auto& obj : m_pickables)
		{
			m_bundles[PipelineMode::Filled].push_back(bundleRecorderFilled.CreateBundleFor(&obj->GetMesh()->m_buffer, m_bundleAllocator.Get()));
		}
		
	}
}

void D3D12HelloWindow::CreateIDRenderTarget()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1; // 단일 RT
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_idRTVHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_idRTVHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, 1, 1);
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&m_idRenderTarget)
	));

	m_device->CreateRenderTargetView(m_idRenderTarget.Get(), nullptr, handle);
	m_idRTVHandle = handle;
	m_idRTState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void D3D12HelloWindow::RenderForIDPass()
{
	m_commandList->SetPipelineState(m_pipelineStates[PipelineMode::PickID].Get());
	m_commandList->SetGraphicsRootSignature(m_idRootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Clear + Bind RTV
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_commandList->OMSetRenderTargets(1, &m_idRTVHandle, FALSE, nullptr);
	m_commandList->ClearRenderTargetView(m_idRTVHandle, clearColor, 0, nullptr);

	// Camera constant buffer
	m_camera->BindConstantBuffer(m_commandList.Get(), 0);

	// ID Color 렌더링 대상 오브젝트
	for (const auto& obj : m_pickables)
	{
		obj->RenderForPicking(m_commandList.Get());
	}
}

uint32_t D3D12HelloWindow::ReadPickedID()
{
	if (!m_pendingPick) return 0;

	m_pendingPick = false;

	uint8_t* mapped = nullptr;
	D3D12_RANGE range = { 0, sizeof(uint32_t) };
	//D3D12_RANGE range = { 0, static_cast<SIZE_T>(m_readbackBufferSize) };
	m_readbackBuffer->Map(0, &range, reinterpret_cast<void**>(&mapped));

	D3D12_RESOURCE_DESC desc = m_idRenderTarget->GetDesc();
	UINT width = static_cast<UINT>(desc.Width);
	UINT height = desc.Height;
	uint32_t rowPitch = m_readbackFootprint.Footprint.RowPitch;
	uint8_t pixel[4];
	memcpy(pixel, mapped, 4);
	uint8_t r = pixel[0], g = pixel[1], b = pixel[2], a = pixel[3];

	m_readbackBuffer->Unmap(0, nullptr);

	return DecodeIDColor(r, g, b, a);
}

uint32_t D3D12HelloWindow::DecodeIDColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	uint32_t scrambled = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
	// 모듈러 역원 (3635633u의 역원): 9121617u
	return (scrambled * 9121617u) & 0xFFFFFF;
}

void D3D12HelloWindow::HandlePickedObject(uint32_t pickedID)
{
	if (pickedID == 0)
	{
		m_selectedObject = nullptr;
		return;
	}

	for (const auto& obj : m_pickables)
	{
		if (obj->GetID() == pickedID)
		{
			m_selectedObject = obj.get();
			return;
		}
	}
}

void D3D12HelloWindow::PrepareReadbackForPicking(int mouseX, int mouseY)
{

	D3D12_RESOURCE_DESC desc = m_idRenderTarget->GetDesc();

	m_device->GetCopyableFootprints(&desc, 0, 1, 0, &m_readbackFootprint, nullptr, nullptr, &m_readbackBufferSize);

	if (!m_readbackBuffer)
	{
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_readbackBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_readbackBuffer)));
	}

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = m_idRenderTarget.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = m_readbackBuffer.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint = m_readbackFootprint;

	D3D12_BOX srcBox = {};
	srcBox.left = mouseX;
	srcBox.right = mouseX + 1;
	srcBox.top = mouseY;
	srcBox.bottom = mouseY + 1;
	srcBox.front = 0;
	srcBox.back = 1;

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_idRenderTarget.Get(),
		m_idRTState,
		D3D12_RESOURCE_STATE_COPY_SOURCE));

	m_idRTState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	
	m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox /*nullptr*/);

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_idRenderTarget.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_idRTState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void D3D12HelloWindow::CreatePipelineStates()
{
	ComPtr<ID3DBlob> vertexShader, pixelShader;
	ComPtr<ID3DBlob> pickIDPS;
	ComPtr<ID3DBlob> highlightVS, highlightPS;

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
	
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"PickID_PS.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pickIDPS, nullptr));
	
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"HighlightPS.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &highlightVS, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetShaderFullPath(L"HighlightPS.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &highlightPS, nullptr));


	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	for (PipelineMode mode : { PipelineMode::Filled, PipelineMode::Line})
	{
		// Create Graphics Pipeline State Object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;

		switch (mode)
		{
		case PipelineMode::Line:
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
		case PipelineMode::Filled:
		default:
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ComPtr <ID3D12PipelineState> pso;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		m_pipelineStates[mode] = pso;
	}

	//Pick ID PSO
	{
		// Root Signature 정의: b0 (ViewProj), b1 (World+Color), Root32bitConstants (object ID color)
		CD3DX12_ROOT_PARAMETER params[3];
		params[0].InitAsConstantBufferView(0); // b0
		params[1].InitAsConstantBufferView(1); // b1
		params[2].InitAsConstants(4, 2, 0); // 4 x 32bit = float4
		//params[2].InitAsConstantBufferView(2); // b2

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(3, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> sigBlob, errBlob;
		D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
		m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_idRootSignature));

		// PSO 설정
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_idRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pickIDPS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStates[PipelineMode::PickID]));
	}

	// Highlight PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };   // VSInput만 있으면 됨
		desc.pRootSignature = m_rootSignature.Get();
		desc.VS = CD3DX12_SHADER_BYTECODE(highlightVS.Get());
		desc.PS = CD3DX12_SHADER_BYTECODE(highlightPS.Get());
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipelineStates[PipelineMode::Highlight])));
	}

}

void D3D12HelloWindow::PopulateCommandList()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	// ID 렌더 타겟을 항상 RENDER_TARGET으로 만들기
	if (m_idRTState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_idRenderTarget.Get(),
			m_idRTState,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_idRTState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	RenderForIDPass();

	if (m_debugViewEnabled)
	{
		// ID 렌더 타겟을 backbuffer로 복사
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_idRenderTarget.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE));
		m_idRTState = D3D12_RESOURCE_STATE_COPY_SOURCE;

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST));

		m_commandList->CopyResource(
			m_renderTargets[m_frameIndex].Get(),
			m_idRenderTarget.Get());

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT));

		ThrowIfFailed(m_commandList->Close());
		return; // 일반 렌더링 스킵
	}
	else if(m_idRTState != D3D12_RESOURCE_STATE_COMMON)
	{
		// ID RT 상태 되돌리기
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_idRenderTarget.Get(),
			m_idRTState,
			D3D12_RESOURCE_STATE_COMMON));
		m_idRTState = D3D12_RESOURCE_STATE_COMMON;
	}

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Bind Camera Constant Buffer
	m_camera->BindConstantBuffer(m_commandList.Get(), 0);

	// 렌더 타겟 생성됨, Back Buffer를 RenderTarget 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// RTV Clear 명령 추가.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// PSO 별로 Draw Command 실행
	for (PipelineMode mode : { PipelineMode::Filled, PipelineMode::Line })
	{
		if (m_bundles[mode].empty()) continue;
		m_commandList->SetPipelineState(m_pipelineStates[mode].Get());
		for (auto bundle : m_bundles[mode])
		{
			// Bundle에 있던 Command 실행.
			m_commandList->ExecuteBundle(bundle.Get());
		}
	}

	// Highlight Pass
	if (m_selectedObject)
	{
		m_commandList->SetPipelineState(m_pipelineStates[PipelineMode::Highlight].Get());
		m_commandList->OMSetStencilRef(1);

		m_selectedObject->GetMesh()->m_buffer.Draw(m_commandList.Get());
	}

	OnRenderUI();

	if (m_pendingPick)
	{
		// Copy the pixel under cursor to readback buffer
		PrepareReadbackForPicking(m_pendingMouseX, m_pendingMouseY); // <-- 새로운 함수
	}

	// 렌더링 끝났음. Present 상태로 전환
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloWindow::WaitForGpu()
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

void D3D12HelloWindow::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}
