#include "pch.h"
#include "MCTerraformEditor.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Math/PhysicsHelper.h"
#include <algorithm>
#include <typeinfo>
#include <iostream>
#ifdef _DEBUG
#include <numeric>
#endif // _DEBUG

int sDebugFrame = 0;
void MCTerraformEditor::OnDestroy()
{
#if PIX_DEBUGMODE
	if (PIXGetCaptureState() == PIX_CAPTURE_GPU)
	{
		PIXEndCapture(FALSE);
	}
#endif

	EditorApp::OnDestroy();
}

void MCTerraformEditor::InitScene(ID3D12GraphicsCommandList* cmd)
{
	m_lightManager->AddDirectional({ -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	m_lightDir = { 1.0f, -1.0f, 0.0f };

	// TerrainSystem 초기화
	{
		GridDesc gridDesc{};
		gridDesc.chunkSize = 50u;
		auto initialSphereField = MakeSphereGrid(100U, 1.0f, 25.0f, m_gridOrigin, gridDesc);
		m_terrain = std::make_unique<TerrainSystem>(m_device.Get(), initialSphereField, gridDesc, TerrainMode::CPU_MC33);
		RemeshRequest req(m_mcIso);

		UINT chunkX = gridDesc.cells.x / gridDesc.chunkSize;
		UINT chunkY = gridDesc.cells.y / gridDesc.chunkSize;
		UINT chunkZ = gridDesc.cells.z / gridDesc.chunkSize;
		for (UINT x = 0; x < chunkX; ++x)
			for (UINT y = 0; y < chunkY; ++y)
				for (UINT z = 0; z < chunkZ; ++z)
					req.chunkset.insert(ChunkKey{ x,y,z });
		m_terrain->requestRemesh(req);

		MeshChunkRenderer* terrainMesh = m_terrain->GetRenderer();
		terrainMesh->SetDebugName("TerrainMesh");
		terrainMesh->SetMaterial(m_defaultMat);
	}

#ifdef _DEBUG	
	// Debug Terrain Cell
	{
		GeometryData debugcellData;
		m_terrain->MakeDebugCell(debugcellData, false);
		std::unique_ptr<Mesh> debugCellMesh = std::make_unique<Mesh>(m_device.Get(), debugcellData);
		debugCellMesh->SetDebugName("TerrainCell");
		m_renderSystem->RegisterStatic(debugCellMesh.get(), "Line");

		m_StaticObjects.push_back(std::move(debugCellMesh));
	}

	// Debug Brush
	{
		GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		m_debugBrush = std::make_unique<Mesh>(m_device.Get(), debugBrushData);
		m_debugBrush->SetMaterial(m_defaultMat);
		m_debugBrush->SetDebugName("DebugBrush");
		m_renderSystem->RegisterDynamic(m_debugBrush.get(), "Wire");
	}
#endif // _DEBUG

}

void MCTerraformEditor::InitUI()
{
	EditorApp::InitUI();
	cameraUIToken = GetUIRenderer()->AddFrameRenderCallbackToken(std::bind(&MCTerraformEditor::RenderCameraLightUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Camera UI"
		});

	marchingCubesUIToken = GetUIRenderer()->AddFrameRenderCallbackToken(std::bind(&MCTerraformEditor::RenderMarchingCubesUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "MarchingCubes UI"
	});
}

void MCTerraformEditor::UpdateScene(float deltaTime)
{
	if (m_inputState.IsPressed(ActionKey::Escape))
	{
		PostQuitMessage(0);
		return;
	}

#ifdef _DEBUG
	if (m_inputState.GetKeyState(ActionKey::ToggleDebugView) == ActionKeyState::JustPressed)
	{
		m_debugViewEnabled = !m_debugViewEnabled;
	}
	else if (m_inputState.GetKeyState(ActionKey::ToggleWireFrame) == ActionKeyState::JustPressed)
	{
		m_wireFrameEnabled = !m_wireFrameEnabled;
		m_renderSystem->SetWireViewEnabled(m_wireFrameEnabled);
	}
	else if (m_inputState.GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustPressed)
	{
		m_debugNormalEnabled = !m_debugNormalEnabled;
	}
#endif // _DEBUG

	if (!m_uiRenderer->IsCapturingUI())
	{
		m_mainCamera->Move(m_inputState, deltaTime);
		if (m_inputState.m_rightBtnState == ActionKeyState::Pressed)
		{
			m_mainCamera->Rotate(m_inputState.m_mouseDeltaX, m_inputState.m_mouseDeltaY);
		}

		// 마우스 좌 버튼 (Terraform)
		const bool terraformHeld = m_inputState.m_leftBtnState == ActionKeyState::Pressed;

		if (terraformHeld)
		{
			MeshChunkRenderer* terrainRenderer = m_terrain->GetRenderer();

			XMVECTOR rayOrigin, rayDir;
			PhysicsUtil::MakeRay(static_cast<float>(m_inputState.m_mouseX), static_cast<float>(m_inputState.m_mouseY), *m_mainCamera.get(), rayOrigin, rayDir);

			XMVECTOR rayOriginls = XMVector3TransformCoord(rayOrigin, terrainRenderer->GetWorldInvMatrix());
			// normalized된 방향 벡터는 회전 행렬에 대한 역변환만 고려하면되지만 편의를 위해 WorldInvMatrix를 사용
			XMVECTOR rayDirls = XMVector3Normalize(XMVector3TransformCoord(rayDir, terrainRenderer->GetWorldInvMatrix()));

			// RayCast로 terrainMesh를 피킹중인지 확인
			XMFLOAT3 hitPosLS;
			if (PhysicsUtil::IsHit(rayOriginls, rayDirls, terrainRenderer->GetCPUData(), terrainRenderer->GetBoundingBox(), hitPosLS))
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
		}
	}

	m_terrain->tryFetch(m_device.Get(), m_renderSystem.get(), "Filled");
}

void MCTerraformEditor::UpdateUI(float deltaTime)
{
	EditorApp::UpdateUI(deltaTime);
	if (m_gridRenewRequested)
	{
		GridDesc gridDesc{};
		gridDesc.cells = { (UINT)m_gridSize[0], (UINT)m_gridSize[1], (UINT)m_gridSize[2] };
		gridDesc.cellsize = static_cast<float>(m_cellSize);
		gridDesc.origin = m_gridOrigin;
		m_terrain->setGridDesc(m_device.Get(), gridDesc);
		m_gridRenewRequested = false;
	}
}

void MCTerraformEditor::BeforeDraw(ID3D12GraphicsCommandList* cmd)
{
	EditorApp::BeforeDraw(cmd);
}

void MCTerraformEditor::DrawScene(ID3D12GraphicsCommandList* cmd)
{
	EditorApp::DrawScene(cmd);

	if (m_debugNormalEnabled)
	{
		PSOList* psoList = m_renderSystem->GetPSOList();
		auto& buckets = m_renderSystem->GetBuckets();

		cmd->SetPipelineState(psoList->Get(psoList->IndexOf("DrawNormal")));
		for (int i = 0; i < psoList->Count(); ++i) {
			auto* pso = psoList->Get(i);
			if (!pso || !m_renderSystem->GetPsoEnabled(i)) continue;

			if (!buckets[i].staticItem.objects.empty())
				cmd->ExecuteBundle(buckets[i].staticItem.bundle);

			for (auto& item : buckets[i].dynamicItems)
				RecordDrawItem(cmd, item.object);
		}
	}
}

//void MCTerraformEditor::DrawUI(ID3D12GraphicsCommandList* cmd)
//{
//	EditorApp::DrawUI(cmd);
//	// Debuging Tools
//#ifdef _DEBUG
//	//auto* allocator = m_renderSystem->GetGpuAllocator();
//	//if (allocator) {
//	//	ImGui::Begin("Buffer Pools Summary");
//	//	ImGui::Text("Pools (Small / Large)"); 
//	//	ImGui::Separator();
//
//	//	static bool showFreeBlocks = true;
//	//	static bool showPromoted = true;
//	//	ImGui::Checkbox("Show Free Blocks", &showFreeBlocks);
//	//	ImGui::SameLine();
//	//	ImGui::Checkbox("Show Promoted", &showPromoted);
//	//	ImGui::Separator();
//
//	//	for (auto& pi : allocator->GetDebugPools())
//	//	{
//	//		if (!pi.pool) continue;
//	//		auto info = pi.pool->GetDebugInfo();
//	//		const double used = info.capacity - std::accumulate(info.freeBlocks.begin(), info.freeBlocks.end(), 0ULL, [](UINT64 sum, const GPUBufferPool::DebugBlock& b) { return sum + b.size; });
//	//		double frag = info.freeBlocks.empty() ? 0.0 : (double)info.freeBlocks.size();
//
//	//		ImGui::Text("%s : Resource %p  Capacity: %llu  Used: %llu  FreeBlocks: %zu",
//	//			pi.name.c_str(), info.resource, (unsigned long long)info.capacity, (unsigned long long)used, info.freeBlocks.size());
//
//	//		// small mini-bar (capacity)
//	//		const float barW = ImGui::GetContentRegionAvail().x;
//	//		const float barH = 14.0f;
//	//		ImVec2 p0 = ImGui::GetCursorScreenPos();
//	//		ImVec2 p1 = ImVec2(p0.x + barW, p0.y + barH);
//	//		auto* dl = ImGui::GetWindowDrawList();
//	//		dl->AddRectFilled(p0, p1, IM_COL32(255, 30, 30, 255), 4.0f);
//	//		dl->AddRect(p0, p1, IM_COL32(200, 200, 200, 128), 4.0f);
//	//		auto toX = [&](UINT64 off)->float { return p0.x + float((double)off / (double)info.capacity * barW); };
//
//	//		if (showFreeBlocks) 
//	//		{
//	//			for (auto& fb : info.freeBlocks) 
//	//			{
//	//				float x0 = toX(fb.offset);
//	//				float x1 = toX(fb.offset + fb.size);
//	//				dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), IM_COL32(120, 120, 120, 160), 2.0f);
//	//			}
//	//		}
//	//		ImGui::Dummy(ImVec2(barW, barH + 6.0f));
//	//	}
//	//	ImGui::End();
//	//}
//#endif // _DEBUG
//}

void MCTerraformEditor::RenderCameraLightUI()
{
	ImGui::Begin("Camera & Light");
	ImGui::Text("Position : %.1f, %.1f, %.1f", m_mainCamera->GetPosition().x, m_mainCamera->GetPosition().y, m_mainCamera->GetPosition().z);
	ImGui::Text("MoveSpeed");
	ImGui::SliderFloat("##MoveSpeed", &m_mainCamera->GetMoveSpeedPtr(), 1.0f, 25.0f);
	ImGui::Text("Light Direction");
	if (ImGui::DragFloat3("## Light Direction", m_lightDir.data(), 0.1f, -1.0f, 1.0f))
	{
		XMFLOAT3 newDir = { -m_lightDir[0], -m_lightDir[1], -m_lightDir[2] };
		m_lightManager->SetDirection(newDir);
	}
	ImGui::End();
}
void MCTerraformEditor::RenderMarchingCubesUI()
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
		GeometryData newBrushMeshData;
		MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		m_debugBrush->SetCPUData(newBrushMeshData);
		m_renderSystem->UpdateDynamic(m_debugBrush.get(), newBrushMeshData);
	}

	ImGui::Text("Brush Strength");
	ImGui::DragFloat("##Brush Strength", &m_brushStrength, 1.0f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

	ImGui::End();
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