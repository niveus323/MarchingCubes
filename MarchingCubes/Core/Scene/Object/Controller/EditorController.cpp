#include "pch.h"
#include "EditorController.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Input/InputState.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/UI/Builder/UIBuilder.h"

BEGIN_REFLECTION(EditorController, Controller)
    REFLECT_PROPERTY(m_cameraSpeed, EPropertyType::Float)
    REFLECT_PROPERTY(m_mouseSensitivity, EPropertyType::Float)
END_REFLECTION()

EditorController::EditorController(Scene* scene) : Controller(scene)
{
}

void EditorController::Update(float deltaTime)
{
    Controller::Update(deltaTime);
    if (m_activeTool) m_activeTool->Update(deltaTime);
}

void EditorController::RenderUI(IUIBuilder* ui)
{
    if (m_activeTool) 
    {
        ui->PushID(m_activeTool.get());
        m_activeTool->RenderUI(ui);
        ui->PopID();
    }
}

void EditorController::SetTool(std::shared_ptr<IEditorTool> newTool)
{
    if (m_activeTool) m_activeTool->OnDeactivated();
    m_activeTool = newTool;
    if (m_activeTool) m_activeTool->OnActivated(this);
}

void EditorController::ProcessInput(float deltaTime)
{
    auto input = EngineCore::GetInputState();
    
    if (input->IsPressed(ActionKey::RightClick))
    {
        UpdateCameraMovement(deltaTime);
        RotateCamera(deltaTime);
    }

    if (m_activeTool)
    {
        m_activeTool->ProcessInput(input, deltaTime);
    }
}

// 공통적인 에디터용 컨트롤러로서 자유 카메라 이동 기능을 제공
void EditorController::UpdateCameraMovement(float deltaTime)
{
   auto input = EngineCore::GetInputState();
   if (!input) return;

   XMFLOAT3 inputDir = { 0.0f, 0.0f, 0.0f };

   if (input->IsPressed(ActionKey::MoveForward)) inputDir.z += 1.0f;
   else if (input->IsPressed(ActionKey::MoveBackward))inputDir.z -= 1.0f;

   if (input->IsPressed(ActionKey::MoveRight)) inputDir.x += 1.0f;
   else if (input->IsPressed(ActionKey::MoveLeft)) inputDir.x -= 1.0f;

   if (input->IsPressed(ActionKey::MoveUp)) inputDir.y += 1.0f;
   else if (input->IsPressed(ActionKey::MoveDown)) inputDir.y -= 1.0f;

   if (inputDir.x == 0.0f && inputDir.y == 0.0f && inputDir.z == 0.0f)
       return;

   XMVECTOR moveInput = XMLoadFloat3(&inputDir);

   XMVECTOR forward = m_possessed->GetForwardVector();
   XMVECTOR right = m_possessed->GetRightVector();
   XMVECTOR up = m_possessed->GetUpVector();

   XMVECTOR worldDirection = (forward * inputDir.z) + (right * inputDir.x) + (up * inputDir.y);
   worldDirection = XMVector3Normalize(worldDirection);

   m_possessed->AddMovementInput(worldDirection, m_cameraSpeed * deltaTime);
}

void EditorController::RotateCamera(float deltaTime)
{
    auto input = EngineCore::GetInputState();
    auto* camera = m_scene->GetMainCamera();
    if (!camera || !input) return;

    AddYawInput(input->GetAxisValue(ActionKey::MouseX));
    AddPitchInput(input->GetAxisValue(ActionKey::MouseY));    
}

void EditorController::AddYawInput(float val)
{
    if (val == 0.0f || !m_possessed) return;
    
    float amount = val * m_mouseSensitivity;
    m_possessed->AddControllerYawInput(amount);
}

void EditorController::AddPitchInput(float val)
{
    if (val == 0.0f || !m_possessed) return;
    
    float amount = val * m_mouseSensitivity;
    m_possessed->AddControllerPitchInput(amount);
}