#include "pch.h"
#include "EditorController.h"
#include "./Win32Application.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Input/InputState.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Scene/Component/BillboardComponent.h"
#include "Core/Math/PhysicsHelper.h"

BEGIN_REFLECTION(EditorController, Controller)
    REFLECT_PROPERTY(m_cameraSpeed, EPropertyType::Float)
    REFLECT_PROPERTY(m_mouseSensitivity, EPropertyType::Float)
END_REFLECTION()

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

void EditorController::SelectObject(GameObject* obj)
{
    if (m_selectedObject == obj) return;

    m_selectedObject = obj;
    if(m_activeTool) m_activeTool->OnSelectionUpdated(obj);
    if (m_selectionCallback) m_selectionCallback(m_selectedObject);
}

void EditorController::SetViewportActive(bool bHovered, bool bFocused)
{
    m_bViewportHovered = bHovered;
    m_bViewportFocused = bFocused;
}

void EditorController::RenderGizmoUI(IUIBuilder* ui)
{
    auto scene = GetScene();
    if (!scene || !m_selectedObject) return;

    auto camera = scene->GetMainCamera();
    auto sceneObj = dynamic_cast<SceneObject*>(m_selectedObject);
    if (!camera || !sceneObj) return;

    TransformComponent* transform = sceneObj->GetTransformComponent();
    if (!transform) return;

    DirectX::XMFLOAT4X4 viewMat, projMat, worldMat;
    DirectX::XMStoreFloat4x4(&viewMat, camera->GetViewMatrix());
    DirectX::XMStoreFloat4x4(&projMat, camera->GetProjMatrix());
    DirectX::XMStoreFloat4x4(&worldMat, sceneObj->GetWorldMatrix());

    DirectX::XMFLOAT3 outPos = transform->GetWorldPosition();
    DirectX::XMFLOAT3 outRot = transform->GetWorldRotation();
    DirectX::XMFLOAT3 outScale = transform->GetWorldScale();
    bool bManipulated = ui->DrawTransformGizmo(
        viewMat,
        projMat,
        m_currentGizmoOperation,
        m_currentGizmoMode,
        worldMat,
        outPos,
        outRot,
        outScale, 
        m_gizmoSize
    );
    m_bIsGizmoHovered = ui->IsGizmoHovered();
    if (bManipulated)
    {
        transform->SetWorldPosition(outPos);
        transform->SetWorldRotation(outRot);
        transform->SetScale(outScale);
    }
}

void EditorController::ProcessInput(float deltaTime)
{
    if (!m_bViewportHovered && !m_bViewportFocused) return;

    auto input = EngineCore::GetInputState();
    bool bInputConsumed = false;
    if (m_activeTool)
    {
        bInputConsumed = m_activeTool->ProcessInput(input, deltaTime);
    }

    if (bInputConsumed) return;

    static bool s_bIsCameraMoving = false;
    if (input->IsPressed(ActionKey::RightClick))
    {
        if (!s_bIsCameraMoving)
        {
            // 우클릭 시작: 커서를 숨기고 마우스를 현재 윈도우 영역에 가둠(Clip)
            HWND hwnd = Win32Application::GetHwnd();
            SetCapture(hwnd);
            ShowCursor(FALSE);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            ClientToScreen(hwnd, (POINT*)&clientRect.left);
            ClientToScreen(hwnd, (POINT*)&clientRect.right);
            ClipCursor(&clientRect);

            s_bIsCameraMoving = true;
        }

        UpdateCameraMovement(deltaTime);
        RotateCamera(deltaTime);
    }
    else
    {
        if (s_bIsCameraMoving)
        {
            // 커서 숨김 해제 및 락(Lock) 풀기
            ReleaseCapture();
            ShowCursor(TRUE);
            ClipCursor(NULL);

            s_bIsCameraMoving = false;
        }
    }

    // Gizmo 모드 변경
    if (!s_bIsCameraMoving)
    {
        if (input->IsPressed(ActionKey::ToggleGizmoTranslation)) m_currentGizmoOperation = UI::EGizmoOperation::Translate;
        else if (input->IsPressed(ActionKey::ToggleGizmoRotation)) m_currentGizmoOperation = UI::EGizmoOperation::Rotate;
        else if (input->IsPressed(ActionKey::ToggleGizmoScaling)) m_currentGizmoOperation = UI::EGizmoOperation::Scale;

        if (!m_bIsGizmoHovered && input->IsPressed(ActionKey::LeftClick))
        {
            auto& mousePos = input->GetMousePos();
            SelectObject(PerformMousePicking(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)));
        }
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
    if (auto input = EngineCore::GetInputState())
    {
        AddYawInput(input->GetAxisValue(ActionKey::MouseX));
        AddPitchInput(input->GetAxisValue(ActionKey::MouseY));
    }
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

GameObject* EditorController::PerformMousePicking(float mouseX, float mouseY)
{
    auto scene = GetScene();
    if (!scene) return nullptr;

    auto camera = scene->GetMainCamera();
    if (!camera) return nullptr;

    float viewportWidth = camera->GetViewportWidth();
    float viewportHeight = camera->GetViewportHeight();
    DirectX::XMMATRIX viewProj = camera->GetViewProjMatrix();
    PhysicsUtil::Ray ray = PhysicsUtil::MakeRay(mouseX, mouseY, viewportWidth, viewportHeight, viewProj);
    GameObject* pickedObject = nullptr;
    PhysicsUtil::HitResult closestHit;
    closestHit.distance = FLT_MAX;
    for (const auto& obj : scene->GetObjects())
    {
        if (obj->HasAnyFlags(EObjectFlags::EditorOnly)) continue; //EditorController, SpectatorPawn 등 에디터 작업을 위한 오브젝트는 제외
        if (auto billboard = obj->GetComponent<BillboardComponent>())
        {
            DirectX::BoundingBox aabb{};
            // Local -> World
            billboard->GetBoundingBox().Transform(aabb, billboard->GetWorldMatrix(camera));

            float distance = 0.0f;
            if (aabb.Intersects(ray.origin, ray.direction, distance))
            {
                if (distance < closestHit.distance)
                {
                    closestHit.distance = distance;
                    pickedObject = obj.get();
                }
            }
        }

        // [B] 추후 MeshComponent 피킹 적용을 위한 구조적 예시
        // PhysicsUtil::IsHit(std::vector<RaycastTarget>, Ray, HitResult) 활용
        /*
        if (auto meshComp = obj->GetComponent<MeshComponent>())
        {
            // MeshComponent에서 Mesh* 를 가져오는 Public Getter가 필요합니다.
            if (Mesh* mesh = meshComp->GetMesh())
            {
                PhysicsUtil::RaycastTarget target{
                    .data = &mesh->GetGeometryData(), // _DEBUG 모드에서 접근 가능
                    .bounds = mesh->GetBounds()[0],   // Broadphase용 메인 Bound
                    .worldMatrix = obj->GetWorldTransform(),
                    .userData = obj.get()
                };

                PhysicsUtil::HitResult meshHit;
                if (PhysicsUtil::IsHit({target}, ray, meshHit))
                {
                    // 빌보드보다 더 가까운 메쉬를 클릭했다면 결과 갱신
                    if (meshHit.distance < closestHit.distance)
                    {
                        closestHit = meshHit;
                        pickedObject = static_cast<GameObject*>(meshHit.userData);
                    }
                }
            }
        }
        */
    }
    return pickedObject;
}
