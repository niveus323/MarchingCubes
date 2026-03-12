#include "pch.h"
#include "EditorController.h"
#include "./Win32Application.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Input/InputState.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Pawn.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Math/PhysicsHelper.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Scene/Component/MeshComponent.h"
#include "Core/Scene/Component/BillboardComponent.h"

BEGIN_REFLECTION(EditorController, Controller)
    REFLECT_PROPERTY(m_cameraSpeed, EPropertyType::Float, "CamSpeed")
    REFLECT_PROPERTY(m_mouseSensitivity, EPropertyType::Float, "Sensitivity")
END_REFLECTION()

void EditorController::Update(float deltaTime)
{
    // MousePicking 결과 반환 (Controller에서 ProcessInput을 호출하므로 Controller보다 먼저 Update필요)
    if (m_pickingFenceValue != 0 && EngineCore::GetSwapChainFence()->GetCompletedValue() >= m_pickingFenceValue)
    {
        XMUINT4 pixel = EngineCore::GetRenderSystem()->GetHitProxyPixel();
        uint32_t resultID = DecodeHitProxyID(pixel);
        Log::Print("EditorController", "Picked Pixel RGB(%u, %u, %u) -> ObjectID : %u", pixel.x, pixel.y, pixel.z, resultID);
        SelectObject(GetScene()->FindObject(resultID));
        m_pickingFenceValue = 0;
    }

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

void EditorController::RenderGizmoOptionUI(IUIBuilder* ui)
{
    // 뷰포트 오버레이 툴바
    UI::Vector<float, 2> toolOverlayPos(0.0f);
    UI::Vector<float, 2> toolOverlaySize(0.0f); // 사이즈 설정하지 않음
    UI::UI_PanelOption toolOverlayFlags =
        UI::UI_PanelOption::NoDecoration |
        UI::UI_PanelOption::AutoResize |
        UI::UI_PanelOption::NoSavedSettings |
        UI::UI_PanelOption::NoFocusOnAppearing |
        UI::UI_PanelOption::NoNav |
        UI::UI_PanelOption::NoMove |
        UI::UI_PanelOption::NoDocking |
        UI::UI_PanelOption::NoBackground;
    ui->BeginOverlay("GizmoOptionOverlay", toolOverlayPos, toolOverlaySize, 0.0f, toolOverlayFlags, UI::UI_AlignmentX::Align_Right);

    // Translate / Rotate / Scale 전환 버튼
    if (ui->Button(m_currentGizmoOperation == UI::EGizmoOperation::Translate ? "[T]" : " T ")) m_currentGizmoOperation = UI::EGizmoOperation::Translate;
    ui->SameLine();
    if (ui->Button(m_currentGizmoOperation == UI::EGizmoOperation::Rotate ? "[R]" : " R ")) m_currentGizmoOperation = UI::EGizmoOperation::Rotate;
    ui->SameLine();
    if (ui->Button(m_currentGizmoOperation == UI::EGizmoOperation::Scale ? "[S]" : " S ")) m_currentGizmoOperation = UI::EGizmoOperation::Scale;
    ui->SameLine(0.0f, 15.0f); // 그룹 간 여백

    // Local / World 축 전환 버튼
    if (ui->Button(m_currentGizmoMode == UI::EGizmoMode::Local ? "Local" : "World"))
    {
        m_currentGizmoMode = (m_currentGizmoMode == UI::EGizmoMode::Local) ? UI::EGizmoMode::World : UI::EGizmoMode::Local;
    }

    ui->SameLine(0.0f, 15.0f);

    // Snap 토글 및 크기 설정
    if (ui->Button(m_bUseSnap ? "Snap [ON]" : "Snap [OFF]")) m_bUseSnap = !m_bUseSnap;

    ui->SameLine();

    if (m_currentGizmoOperation == UI::EGizmoOperation::Translate)   ui->Drag("SnapT", &m_snapTranslation);
    else if (m_currentGizmoOperation == UI::EGizmoOperation::Rotate) ui->Drag("SnapR", &m_snapRotation);
    else if (m_currentGizmoOperation == UI::EGizmoOperation::Scale)  ui->Drag("SnapS", &m_snapScale);

    ui->SameLine(0.0f, 15.0f);
    // 에디터 카메라 이동 속도 설정
    ui->Drag("CamSpeed", &m_cameraSpeed);
    ui->EndOverlay();
}

void EditorController::RenderGizmoUI(IUIBuilder* ui)
{
    auto scene = GetScene();
    if (!scene) return;

    // 메인 Gizmo 렌더링
    if (!m_selectedObject) return;

    // 편집 모드에 따른 snap 값 설정
    float snapValues[3] = { 0.0f, 0.0f, 0.0f };
    if (m_bUseSnap)
    {
        if (m_currentGizmoOperation == UI::EGizmoOperation::Translate) { snapValues[0] = snapValues[1] = snapValues[2] = m_snapTranslation; }
        else if (m_currentGizmoOperation == UI::EGizmoOperation::Rotate) { snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation; }
        else if (m_currentGizmoOperation == UI::EGizmoOperation::Scale) { snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale; }
    }

    TransformComponent* transform = m_selectedObject->GetComponent<TransformComponent>();
    if (!transform || !m_targetCam) return;

    DirectX::XMFLOAT4X4 viewMat, projMat, worldMat;
    DirectX::XMStoreFloat4x4(&viewMat, m_targetCam->GetViewMatrix());
    DirectX::XMStoreFloat4x4(&projMat, m_targetCam->GetProjMatrix());
    DirectX::XMStoreFloat4x4(&worldMat, transform->GetWorldMatrix());

    DirectX::XMFLOAT3 outPos = transform->GetWorldPosition();
    DirectX::XMFLOAT3 outRot = transform->GetWorldRotation();
    DirectX::XMFLOAT3 outScale = transform->GetWorldScale();
    bool bManipulated = ui->DrawTransformGizmo(
        viewMat, projMat,
        m_currentGizmoOperation, m_currentGizmoMode,
        worldMat,
        outPos, outRot, outScale, 
        m_bUseSnap ? snapValues : nullptr, 
        m_gizmoSize);
    m_bIsGizmoHovered = ui->IsGizmoHovered();
    if (bManipulated)
    {
        switch (m_currentGizmoOperation)
        {
            case UI::EGizmoOperation::Translate:
            {
                if (m_currentGizmoMode == UI::EGizmoMode::World)
                    transform->SetWorldPosition(outPos);
                else
                    transform->SetPosition(outPos);
            }
            break;
            case UI::EGizmoOperation::Rotate:
            {
                if (m_currentGizmoMode == UI::EGizmoMode::World)
                    transform->SetWorldRotation(outRot);
                else
                    transform->SetRotation(outRot);
            }
            break;
            case UI::EGizmoOperation::Scale:
            {
                if (m_currentGizmoMode == UI::EGizmoMode::World)
                    transform->SetWorldScale(outScale);
                else
                    transform->SetScale(outScale);
            }
            break;
            default:
            break;
        }
    }
}

PhysicsUtil::Ray EditorController::GetViewportMouseRay()
{
    auto camera = GetPossessdCamera();
    if(!camera) return PhysicsUtil::Ray();

    // NOTE : Input의 mousePos는 클라이언트 좌표계.
    auto input = EngineCore::GetInputState();
    MousePos clientMouse = input->GetMousePos();

    // Client 좌표계 -> 뷰포트 이미지의 좌표계
    float localMouseX = static_cast<float>(clientMouse.x) - m_viewportX;
    float localMouseY = static_cast<float>(clientMouse.y) - m_viewportY;
    return PhysicsUtil::MakeRay(localMouseX, localMouseY, m_viewportWidth, m_viewportHeight, camera->GetViewProjMatrix());
}

void EditorController::ProcessInput(float deltaTime)
{
    if (!m_bInputEnabled) return;
    if (!m_bViewportHovered && !m_bViewportFocused) return;

    auto input = EngineCore::GetInputState();
    bool bInputConsumed = false;
    if (m_activeTool && !m_bIsGizmoHovered) // Gizmo 조작을 위해 Gizmo에 Hover하는 경우에는 Gizmo를 우선시한다
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
            Win32Application::CaptureMouseInScreen(true);
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
            Win32Application::CaptureMouseInScreen(false);
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
            // Client 좌표계 -> 뷰포트 이미지의 좌표계
            MousePos clientMouse = input->GetMousePos();
            uint32_t viewportMouseX = clientMouse.x - static_cast<uint32_t>(m_viewportX);
            uint32_t viewportMouseY = clientMouse.y - static_cast<uint32_t>(m_viewportY);
            Log::Print("EditorController", "Trying Pick Objet ViewportPos(%u, %u)", viewportMouseX, viewportMouseY);
            m_pickingFenceValue = EngineCore::GetRenderSystem()->RequestPicking(viewportMouseX, viewportMouseY);
            // 여기서 Fence값을 발급 받아야한다
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

   if (inputDir.x == 0.0f && inputDir.y == 0.0f && inputDir.z == 0.0f) return;

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

//GameObject* EditorController::PerformMousePicking(float mouseX, float mouseY)
//{
//    auto scene = GetScene();
//    if (!scene || !m_targetCam) return nullptr;
//
//    DirectX::XMMATRIX viewProj = m_targetCam->GetViewProjMatrix();
//    auto ray = GetViewportMouseRay();
//    GameObject* pickedObject = nullptr;
//    PhysicsUtil::HitResult closestHit;
//    closestHit.distance = FLT_MAX;
//    std::vector<PhysicsUtil::RaycastTarget> meshTargets;
//    for (const auto& obj : scene->GetObjects())
//    {
//        if (obj->HasAnyFlags(EObjectFlags::EditorOnly)) continue; //EditorController, SpectatorPawn 등 에디터 작업을 위한 오브젝트는 제외
//        if (auto billboard = obj->GetComponent<BillboardComponent>())
//        {
//            DirectX::BoundingOrientedBox obb = billboard->GetBoundingBox(m_targetCam);
//            float distance = 0.0f;
//            if (obb.Intersects(ray.origin, ray.direction, distance))
//            {
//                if (distance < closestHit.distance)
//                {
//                    closestHit.distance = distance;
//                    pickedObject = obj.get();
//                }
//            }
//        }
//
//        // Mesh가 존재할 경우 mesh에 대한 마우스 피킹도 고려
//        if (auto meshComp = obj->GetComponent<MeshComponent>())
//        {
//            if (Mesh* mesh = meshComp->GetMesh())
//            {
//                meshTargets.push_back(PhysicsUtil::RaycastTarget{
//                    .data = &mesh->GetGeometryData(), // 에디터 환경에서만 이 함수를 실행하므로 GetGeometryData호출도 안전함.
//                    .bounds = mesh->GetBounds()[0],
//                    .worldMatrix = meshComp->GetTransformComp()->GetWorldMatrix(),
//                    .userData = obj.get()
//                });
//            }
//        }
//    }
//
//    // Mesh 피킹까지 고려하여 가장 가까운 피킹 결과를 리턴
//    if (!meshTargets.empty())
//    {
//        PhysicsUtil::HitResult hitResult;
//        if (PhysicsUtil::IsHit(meshTargets, ray, hitResult))
//        {
//            // 빌보드보다 더 가까운 메쉬를 클릭했다면 결과 갱신
//            if (hitResult.distance < closestHit.distance)
//            {
//                closestHit = hitResult;
//                pickedObject = static_cast<GameObject*>(hitResult.userData);
//            }
//        }
//    }
//    Log::Print("EditorController", "PickingResult : %s", pickedObject? pickedObject->GetName().c_str() : "Null");
//    return pickedObject;
//}

uint32_t EditorController::DecodeHitProxyID(const DirectX::XMUINT4& pixel)
{
    // (0,0,0)이면 굳이 연산할 필요 없이 0
    if (pixel.x == 0 && pixel.y == 0 && pixel.z == 0) return 0;

    uint32_t id = 0;
    uint32_t bitPos = 0;

    // 역으로 각 채널의 i번째 비트를 추출하여 id의 순차적인 위치(bitPos)에 조립
    for (int i = 7; i >= 0; --i)
    {
        id |= ((pixel.x >> i) & 1) << bitPos++;
        id |= ((pixel.y >> i) & 1) << bitPos++;
        id |= ((pixel.z >> i) & 1) << bitPos++;
    }

    return id;
}
