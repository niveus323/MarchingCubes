#include "pch.h"
#include "Scene_Terraform.h"
#include "Core/UI/ImGUIRenderer.h"
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/Scene/Component/LightComponent.h"
#include "Core/Scene/Object/GameMode/GameMode.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Contents/Scene/Terraform/TerraformTool.h"
#include <algorithm>

Scene_Terraform::Scene_Terraform() : Scene()
{
	m_name = "Scene_Terraform";
}

void Scene_Terraform::Init()
{
	Scene::Init();

	m_terrainSystem = AddSubsystem<TerrainSystem>();
	if (!m_bLoadedFromFile)
	{
		m_directionalLight = CreateObject<SceneObject>("Light0");
		m_directionalLight->SetRotation(XMFLOAT3{ 45.0f, 45.0f, 0.0f });
		m_directionalLight->AddComponent<LightComponent>();
		m_bLoadedFromFile = false;
	}
}

void Scene_Terraform::InitUI(IUIRenderer* ui)
{
	Scene::InitUI(ui);
}

void Scene_Terraform::BeginEditor()
{
	Scene::BeginEditor();

	/*if (auto* editorController = FindObject<EditorController>())
	{
		m_terraformTool = std::make_shared<TerraformTool>(m_terrainSystem);
		editorController->SetTool(m_terraformTool);	
	}*/
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