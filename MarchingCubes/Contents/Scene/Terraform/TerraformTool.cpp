#include "pch.h"
#include "TerraformTool.h"
#include "Core/Input/InputState.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/SceneObject.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include "Core/Geometry/Mesh/MeshChunkRenderer.h"
#include "Core/Geometry/MeshGenerator.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Engine/EngineCore.h"
#include "Core/UI/Builder/UIBuilder.h"

void TerraformTool::OnActivated(EditorController* controller)
{
	IEditorTool::OnActivated(controller);

	// Debug Terrain Cell 생성
	GeometryData debugcellData;
	m_terrainSystem->MakeDebugCell(debugcellData, false);
	m_cellMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugcellData, "TerrainCell");
	m_debugCell = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugCell->AddComponent<MeshComponent>(m_cellMesh.get(), "Line");

	// Debug Brush 생성
	GeometryData debugBrushData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
	m_brushMesh = std::make_unique<Mesh>(EngineCore::GetUploadContext(), debugBrushData, "DebugBrush");
	m_debugBrush = m_owner->GetScene()->CreateObject<SceneObject>();
	m_debugBrush->AddComponent<MeshComponent>(m_brushMesh.get(), "Wire");

}

void TerraformTool::OnDeactivated()
{

	IEditorTool::OnDeactivated();
}

void TerraformTool::ProcessInput(const InputState* input, float deltaTime)
{
	if (input->IsPressed(ActionKey::LeftClick))
	{
		auto* camera = m_owner->GetScene()->GetMainCamera();

		MeshChunkRenderer* terrainRenderer = m_terrainSystem->GetRenderer();

		MousePos mouse = input->GetMousePos();
		float mouseX = static_cast<float>(mouse.x);
		float mouseY = static_cast<float>(mouse.y);

		auto ray = PhysicsUtil::MakeRay(mouseX, mouseY,
			camera->GetViewportWidth(),
			camera->GetViewportHeight(),
			camera->GetViewProjMatrix());

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
		PhysicsUtil::RaycastHitResult result;
		if (PhysicsUtil::IsHit(targets, ray, result))
		{
			const auto& hitPos = result.hitPos;
#ifdef _DEBUG
			// Hit가 발생한 위치에 원 세팅
			m_debugBrush->SetPosition(hitPos);
#endif // DEBUG
			XMVECTOR vHitposLS = XMVector3TransformCoord(XMLoadFloat3(&hitPos), XMMatrixInverse(nullptr, m_terrainRenderer->GetWorldMatrix()));
			XMFLOAT3 hitposLS;
			XMStoreFloat3(&hitposLS, vHitposLS);

			BrushRequest req_brush{
				.deltaTime = deltaTime,
				.center = hitposLS,
				.radius = m_brushRadius,
				.weight = m_brushStrength * (EngineCore::GetInputState()->IsPressed(ActionKey::Ctrl) ? -1.0f : 1.0f)
			};
			m_terrainSystem->RequestBrush(req_brush);
		}
	}

}

void TerraformTool::RenderUI(IUIBuilder* ui)
{
	ui->BeginPanel("Terraform Tool");
	ui->BeginTable("Brush", 2);
	
	//ImGui::DragFloat("##Brush Radius", &m_brushRadius, 0.2f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (ui->PropertyFloat("Brush Radius", &m_brushRadius, 0.2f))
	{
		SetBrushRadius(m_brushRadius);
	}

	//ImGui::DragFloat("##Brush Strength", &m_brushStrength, 1.0f, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (ui->PropertyFloat("Brush Strength", &m_brushStrength, 1.0f))
	{
		SetBrushStrength(m_brushStrength);
	}

	ui->EndTable();
	ui->EndPanel();
}

void TerraformTool::SetBrushRadius(float radius)
{
	m_brushRadius = radius;
	if (m_debugBrush)
	{
		GeometryData newBrushMeshData = MeshGenerator::CreateSphereMeshData(m_brushRadius, { 1.0f, 0.0f, 0.0f, 0.4f });
		m_brushMesh->UpdateData(EngineCore::GetUploadContext(), newBrushMeshData);
	}
}
