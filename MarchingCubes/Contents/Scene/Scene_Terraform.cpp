#include "pch.h"
#include "Scene_Terraform.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Geometry/MarchingCubes/TerrainRendererComponent.h"
#include "Core/Input/InputState.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/LightComponent.h"
#include "Core/Scene/Object/GameMode.h"
#include "Core/Scene/Object/PlayerController.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include <algorithm>

Scene_Terraform::Scene_Terraform() :
	Scene()
{
}

void Scene_Terraform::Init()
{
	Scene::Init();
	if (auto* pc = GetGameMode()->GetPlayerController())
	{
		Pawn* pawn = pc->GetPawn();
		pawn->SetPosition({ 0.0f, 10.0f, -50.0f });
		auto cameraComp = pawn->AddComponent<CameraComponent>(m_viewportWidth, m_viewportHeight);
		SetMainCamera(cameraComp);

		m_cameraSpeed = pc->GetMoveSpeed();
	}

	m_directionalLight = CreateObject<SceneObject>();
	m_directionalLight->SetRotation({ 45.0f, 45.0f, 0.0f });
	m_directionalLight->AddComponent<LightComponent>(ELightType::Directional);
	XMFLOAT3 lightDir = m_directionalLight->GetTransformComponent()->GetForward();
	m_lightDir = { lightDir.x, lightDir.y, lightDir.z };

	// TerrainSystem 초기화
	{
		GridDesc gridDesc{};
		gridDesc.chunkSize = 50u;
		auto initialSphereField = MakeSphereGrid(100U, 1.0f, 25.0f, m_gridOrigin, gridDesc);
		TerrainSystem::InitInfo terrainInfo{
			.device = EngineCore::GetDevice(),
			.grid = initialSphereField,
			.desc = gridDesc,
			.mode = TerrainMode::CPU_MC33,
			.descriptorAllocator = EngineCore::GetDescriptorAllocator(),
			.uploadContext = EngineCore::GetUploadContext()
		};
		m_terrain = std::make_unique<TerrainSystem>(terrainInfo);
		m_terrain->requestRemesh(EngineCore::GetFrameIndex(), m_mcIso);
	}

	// Terrain Object 생성
	{
		m_terrainRenderer = CreateObject<SceneObject>();
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
		auto mesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugcellData, "TerrainCell");
		m_debugCell = CreateObject<SceneObject>();
		auto* meshComp = m_debugCell->AddComponent<MeshComponent>(mesh.get(), "Line");
		m_editorMeshes.push_back(std::move(mesh));
	}

	// Debug Brush 생성
	{
		GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		auto mesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugBrushData, "DebugBrush");
		m_debugBrush = CreateObject<SceneObject>();
		auto* meshComp = m_debugBrush->AddComponent<MeshComponent>(mesh.get(), "Wire");
		m_editorMeshes.push_back(std::move(mesh));
	}
#endif // _DEBUG

}

void Scene_Terraform::InitUI(IUIRenderer* ui)
{
	Scene::InitUI(ui);
	cameraUIToken = ui->AddFrameRenderCallbackToken(std::bind(&Scene_Terraform::RenderCameraLightUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "Camera UI"
		});

	marchingCubesUIToken = ui->AddFrameRenderCallbackToken(std::bind(&Scene_Terraform::RenderMarchingCubesUI, this), UI::UICallbackOptions{
		.priority = 0,
		.rateHz = 0,
		.enabled = true,
		.id = "MarchingCubes UI"
		});
}

void Scene_Terraform::Update(float deltaTime)
{
	Scene::Update(deltaTime);
	// 마우스 좌 버튼 (Terraform)
	const bool terraformHeld = EngineCore::GetInputState()->m_leftBtnState == ActionKeyState::Pressed;
	if (terraformHeld)
	{
		MeshChunkRenderer* terrainRenderer = m_terrain->GetRenderer();

		float mouseX = static_cast<float>(EngineCore::GetInputState()->m_mouseX);
		float mouseY = static_cast<float>(EngineCore::GetInputState()->m_mouseY);

		XMVECTOR rayOrigin, rayDir;
		PhysicsUtil::MakeRay(mouseX, mouseY, 
			m_mainCamera->GetViewportWidth(), 
			m_mainCamera->GetViewportHeight(), 
			m_mainCamera->GetViewProjMatrix(), 
			rayOrigin, rayDir);

		std::vector<PhysicsUtil::RaycastTarget> targets;
		auto& terrainChunks = terrainRenderer->GetChunkSlots();
		for (const auto& chunk : terrainChunks)
		{
			targets.push_back(PhysicsUtil::RaycastTarget{
				.data = &chunk->meshData,
				.bounds = chunk->bounds,
				.worldMatrix = m_terrainRenderer->GetWorldMatrix()
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
			XMVECTOR vHitposLS = XMVector3TransformCoord(XMLoadFloat3(&hitPos), XMMatrixInverse(nullptr, m_terrainRenderer->GetWorldMatrix()));
			XMFLOAT3 hitposLS;
			XMStoreFloat3(&hitposLS, vHitposLS);

			BrushRequest req_brush{
				.hitpos = hitposLS,
				.radius = m_brushRadius,
				.weight = m_brushStrength * (EngineCore::GetInputState()->IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f),
				.deltaTime = deltaTime,
				.isoValue = m_mcIso
			};
			m_terrain->requestBrush(EngineCore::GetFrameIndex(), req_brush);
		}
	}
	m_terrain->tryFetch();
}

// SceneObject를 제외한 렌더 타이밍에 필요한 작업은 이쪽으로
void Scene_Terraform::Render()
{
	Scene::Render();
}

void Scene_Terraform::RenderCameraLightUI()
{
	ImGui::Begin("Camera & Light");
	XMFLOAT3 cameraPos = m_mainCamera->GetOwner<SceneObject>()->GetPosition();
	ImGui::Text("Position : %.1f, %.1f, %.1f", cameraPos.x, cameraPos.y, cameraPos.z);
	ImGui::Text("MoveSpeed");
	if(ImGui::DragFloat("##MoveSpeed", &m_cameraSpeed, 10.0f, 50.0f, 500.0f))
	{
		GetGameMode()->GetPlayerController()->SetMoveSpeed(m_cameraSpeed);
	}
	ImGui::Text("Light Direction");
	if (ImGui::DragFloat3("## Light Direction", m_lightDir.data(), 0.1f, -1.0f, 1.0f))
	{
		//Forward
		XMFLOAT3 newDir = { m_lightDir[0], m_lightDir[1], m_lightDir[2] };
		m_directionalLight->GetTransformComponent()->LookTo(newDir);
	}
	ImGui::End();
}

void Scene_Terraform::RenderMarchingCubesUI()
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
		GeometryData newBrushMeshData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		if (m_debugBrush)
		{
			for (auto& mesh : m_editorMeshes)
			{
				if (mesh->GetDebugName() == "DebugBrush")
				{
					mesh->UpdateData(EngineCore::GetUploadContext(), newBrushMeshData);
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
		m_terrain->setGridDesc(EngineCore::GetDevice(), gridDesc);
		m_terrain->setField(EngineCore::GetDevice(), newSdf);
		m_terrain->requestRemesh(EngineCore::GetFrameIndex(), m_mcIso);

		m_debugCell->SetPosition(m_gridOrigin);
	}
	ImGui::End();
}

std::shared_ptr<SdfField<float>> Scene_Terraform::MakeSphereGrid(unsigned int N, float cellSize, float radius, XMFLOAT3 center, GridDesc& OutGridDesc)
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
