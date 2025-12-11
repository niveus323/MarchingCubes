#include "pch.h"
#include "MCTerraformEditor.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Scene/BaseScene.h"
#include "Core/Assets/ResourceManager.h"
#include "Core/Assets/MeshAsset.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Geometry/MarchingCubes/TerrainRendererComponent.h"
#include "Core/Rendering/RenderSystem.h"
#include <algorithm>

void MCTerraformEditor::OnDestroy()
{
	EditorApp::OnDestroy();
}

void MCTerraformEditor::InitScene(ID3D12GraphicsCommandList* cmd)
{
	m_scene = std::make_unique<BaseScene>(m_renderSystem.get());

	m_lightManager->AddDirectional({ -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	m_lightDir = { 1.0f, -1.0f, 0.0f };

	// TerrainSystem 초기화
	{
		GridDesc gridDesc{};
		gridDesc.chunkSize = 50u;
		auto initialSphereField = MakeSphereGrid(100U, 1.0f, 25.0f, m_gridOrigin, gridDesc);
		TerrainSystem::InitInfo terrainInfo{
			.device = m_device.Get(),
			.grid = initialSphereField,
			.desc = gridDesc,
			.mode = TerrainMode::CPU_MC33,
			.descriptorAllocator = GetDescriptorAllocator(),
			.uploadContext = GetUploadContext()
		};
		m_terrain = std::make_unique<TerrainSystem>(terrainInfo);
		m_terrain->requestRemesh(m_frameIndex, m_mcIso);
	}

	// Terrain Object 생성
	{
		m_terrainRenderer = m_scene->CreateObject<SceneObject>();
		m_terrainRenderer->SetPosition(m_gridOrigin);
		auto* terrainComponent = m_terrainRenderer->AddComponent<TerrainRendererComponent>();
		terrainComponent->SetChunkRenderer(m_terrain->GetRenderer());
		terrainComponent->SetMaterial(0);
	}
#ifdef _DEBUG	
	// Debug Terrain Cell 생성
	{
		GeometryData debugcellData;
		m_terrain->MakeDebugCell(debugcellData, false);
		auto mesh = std::make_unique<Mesh>(GetUploadContext(), debugcellData, "TerrainCell");
		m_debugCell = m_scene->CreateObject<SceneObject>();
		auto* meshComp = m_debugCell->AddComponent<MeshComponent>(mesh.get(), "Line");
		m_editorMeshes.push_back(std::move(mesh));
	}

	// Debug Brush 생성
	{
		GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		auto mesh = std::make_unique<Mesh>(GetUploadContext(), debugBrushData, "DebugBrush");
		m_debugBrush = m_scene->CreateObject<SceneObject>();
		auto* meshComp = m_debugBrush->AddComponent<MeshComponent>(mesh.get(), "Wire");
		m_editorMeshes.push_back(std::move(mesh));
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
		SetDebugViewMode(m_hDefaultView);
	}
	else if (m_inputState.GetKeyState(ActionKey::ToggleWireFrame) == ActionKeyState::JustPressed)
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
	else if (m_inputState.GetKeyState(ActionKey::ToggleDebugNormal) == ActionKeyState::JustPressed)
	{
		SetDebugViewMode(m_hNormalView); // Just Toggle
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

			std::vector<PhysicsUtil::RaycastTarget> targets;
			auto& terrainChunks = terrainRenderer->GetChunkSlots();
			for (const auto& chunk : terrainChunks)
			{
				targets.push_back(PhysicsUtil::RaycastTarget{
					.data = &chunk.meshData,
					.bounds = chunk.bounds,
					.worldMatrix = m_terrainRenderer->GetTransformMatrix()
				});
			}

			// RayCast로 terrainMesh를 피킹중인지 확인
			XMFLOAT3 hitPos{};
			if (PhysicsUtil::IsHit(targets, rayOrigin, rayDir, hitPos))
			{
#ifdef _DEBUG
				// Hit가 발생한 위치에 원 세팅
				m_debugBrush->SetPosition(hitPos);
#endif // DEBUG
				XMVECTOR vHitposLS = XMVector3TransformCoord(XMLoadFloat3(&hitPos), XMMatrixInverse(nullptr, m_terrainRenderer->GetTransformMatrix()));
				XMFLOAT3 hitposLS;
				XMStoreFloat3(&hitposLS, vHitposLS);

				BrushRequest req_brush{
					.hitpos = hitposLS,
					.radius = m_brushRadius,
					.weight = m_brushStrength * (m_inputState.IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f),
					.deltaTime = deltaTime,
					.isoValue = m_mcIso
				};
				m_terrain->requestBrush(m_frameIndex, req_brush);
			}
		}
	}

	m_terrain->tryFetch();
	if (m_scene) m_scene->Update(deltaTime);
}

void MCTerraformEditor::OnUpload(ID3D12GraphicsCommandList* cmd)
{
	EditorApp::OnUpload(cmd);
	if (m_scene) m_scene->Render();
}

void MCTerraformEditor::DrawScene(ID3D12GraphicsCommandList* cmd)
{
	EditorApp::DrawScene(cmd);
}

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
	ImGui::InputFloat3("##Origin", &m_gridOrigin.x);

	ImGui::Text("Num Of Tiles");
	if (ImGui::InputInt("##Num Of Tiles", &m_gridTiles, 1, 25))
	{
		m_gridTiles = std::clamp(m_gridTiles, 1, 100);
	}

	ImGui::Text("Cell Size");
	if (ImGui::InputInt("##Num Of Tiles", &m_cellSize, 1, 4, ImGuiInputTextFlags_AlwaysOverwrite))
	{
		m_cellSize = std::clamp(m_cellSize, 1, 16);
	}

	ImGui::Text("Brush Radius");
	ImGui::DragFloat("##Brush Radius", &m_brushRadius, 0.2f, 3.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		GeometryData newBrushMeshData =	MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		if (m_debugBrush)
		{
			for (auto& mesh : m_editorMeshes) 
			{
				if (mesh->GetDebugName() == "DebugBrush") 
				{
					mesh->UpdateData(GetUploadContext(), newBrushMeshData);
					break;
				}
			}
		}
	}

	ImGui::Text("Brush Strength");
	ImGui::DragFloat("##Brush Strength", &m_brushStrength, 1.0f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::Separator();
	if (ImGui::Button("Generate"))
	{
		GridDesc gridDesc{ .chunkSize = 50u };
		auto newSdf = MakeSphereGrid(m_gridTiles, static_cast<float>(m_cellSize), 25.0f, m_gridOrigin, gridDesc);
		m_terrain->setGridDesc(m_device.Get(), gridDesc);
		m_terrain->setField(m_device.Get(), newSdf);
		m_terrain->requestRemesh(m_frameIndex, m_mcIso);

		m_debugCell->SetPosition(m_gridOrigin);
	}
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

	// 채우기: F = brushRadius - |p - center|
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