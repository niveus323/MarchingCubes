#include "pch.h"
#include "Scene_Terraform.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/MarchingCubes/TerrainRendererComponent.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Scene/Component/LightComponent.h"
#include "Core/Scene/Object/GameMode/GameMode.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Contents/Scene/Terraform/TerraformTool.h"
#include <algorithm>

Scene_Terraform::Scene_Terraform() :
	Scene()
{
}

void Scene_Terraform::Init()
{
	Scene::Init();

	m_directionalLight = CreateObject<SceneObject>();
	m_directionalLight->SetRotation(XMFLOAT3{ 45.0f, 45.0f, 0.0f });
	m_directionalLight->AddComponent<LightComponent>(ELightType::Directional);
	m_terrainSystem = AddSubsystem<TerrainSystem>();
}

void Scene_Terraform::InitUI(IUIRenderer* ui)
{
	Scene::InitUI(ui);

	marchingCubesUIToken = ui->AddFrameRenderCallbackToken(std::bind(&Scene_Terraform::RenderMarchingCubesUI, this), UI::UICallbackOptions{
		.layer = UI::EUILayer::Editor_Panel,
		.rateHz = 0,
		.enabled = true,
		.id = "MarchingCubes UI"
		});
}

void Scene_Terraform::BeginEditor()
{
	Scene::BeginEditor();

	GridDesc gridDesc{};
	gridDesc.chunkSize = 50u;
	gridDesc.isoValue = m_mcIso;
	auto initialSphereField = MakeSphereGrid(100U, 1.0f, 25.0f, m_gridOrigin, gridDesc);
	m_terrainSystem->LoadTerrain(TerrainMode::CPU_MC33, gridDesc, initialSphereField);

	// Terrain Object 생성
	m_terrainRenderer = CreateObject<SceneObject>();
	m_terrainRenderer->SetPosition(m_gridOrigin);
	auto* terrainComponent = m_terrainRenderer->AddComponent<TerrainRendererComponent>();
	terrainComponent->SetChunkRenderer(m_terrainSystem->GetRenderer());
	terrainComponent->SetMaterial(0);

	if (auto* editorController = FindObject<EditorController>())
	{
		Pawn* pawn = editorController->GetPawn();
		pawn->SetPosition({ 0.0f, 0.0f, -120.0f });

		m_terraformTool = std::make_shared<TerraformTool>(m_terrainSystem, m_terrainRenderer);
		editorController->SetTool(m_terraformTool);	
	}
}

void Scene_Terraform::Update(float deltaTime)
{
	Scene::Update(deltaTime);
}

// SceneObject를 제외한 렌더 타이밍에 필요한 작업은 이쪽으로
void Scene_Terraform::Render()
{
	Scene::Render();
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

	ImGui::Separator();
	if (ImGui::Button("Generate"))
	{
		GridDesc gridDesc{ .chunkSize = 50u };
		auto newSdf = MakeSphereGrid(m_gridTiles, static_cast<float>(m_cellSize), 25.0f, m_gridOrigin, gridDesc);
		m_terrainSystem->SetGridDesc(gridDesc);
		m_terrainSystem->SetField(newSdf);
		m_terrainSystem->RequestRemesh();
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

	return std::shared_ptr<SdfField<float>>(gridData);
}
