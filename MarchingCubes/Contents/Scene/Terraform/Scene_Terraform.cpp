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
}

void Scene_Terraform::BeginEditor()
{
	Scene::BeginEditor();

	// Terrain Object 생성
	m_terrainRenderer = CreateObject<SceneObject>();
	m_terrainRenderer->SetPosition(0.0f, 0.0f, 0.0f);
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