#include "pch.h"
#include "ViewportPanel.h"
#include "../Interface/EditorApp.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Object/SpectatorPawn.h"
#include "Core/Scene/Component/CameraComponent.h"
#include "Core/Input/InputState.h"

ViewportPanel::ViewportPanel(EditorApp* app) : IEditorPanel(app)
{
    m_bAllowEngineInput = true;
}

void ViewportPanel::OnUpdate(float deltaTime)
{
    auto it = m_onScreenMessages.begin();
    while (it != m_onScreenMessages.end())
    {
        it->timeRemaining -= deltaTime;
        if (it->timeRemaining <= 0.0f)
        {
            it = m_onScreenMessages.erase(it); // 시간 만료 시 삭제
        }
        else
        {
            ++it;
        }
    }
}

void ViewportPanel::OnRenderUI(IUIBuilder* ui)
{
    ui->PushStyle_Padding({ 0.0f, 0.0f });
    ui->SetNextWindowDocking("Main DockSpace", UI::UI_Condition::FirstUseEver, UI::UI_DockingOption::NoTabBar | UI::UI_DockingOption::NoUndocking);
    if (ui->BeginPanel("Viewport", NULL, UI::UI_PanelOption::NoTitleBar | UI::UI_PanelOption::NoCollapse | UI::UI_PanelOption::NoScrollBar))
    {
        m_bIsFocused = ui->IsWindowFocused();
        m_bIsHovered = ui->IsWindowHovered();
        bool bResized = false;

        auto viewportPanelSize = ui->GetRegionAvailable();
        auto p0 = ui->GetCursorScreenPos();
        if (m_viewportSize.x != viewportPanelSize.x || m_viewportSize.y != viewportPanelSize.y)
        {
            bResized = true;
            m_viewportSize = viewportPanelSize;
            if (m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)  m_onViewportResized(m_viewportSize);
        }

        uint64_t srvGPUHandle = m_ownerApp->GetOffscreenSRVGpuHandle().ptr;

        if (srvGPUHandle != 0 && m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)
        {
            if (m_bHitProxyDebugMode)
            {
                // HitProxy 렌더링
                auto hitProxySRV = EngineCore::GetRenderSystem()->GetHitProxySRV();
                ui->Image((void*)hitProxySRV.ptr, m_viewportSize);
            }
            else
            {
                // 메인 렌더링
                void* textureID = reinterpret_cast<void*>(srvGPUHandle);
                ui->Image(textureID, m_viewportSize);
            }
        }
        else
        {
            auto p0 = ui->GetCursorScreenPos();
            ui->Dummy(m_viewportSize);
            ui->DrawRectFilled(p0, { p0.x + m_viewportSize.x, p0.y + m_viewportSize.y }, { 0.1f, 0.1f, 0.1f, 1.0f });
        }

        // 뷰포트용 ToolBar UI 렌더링
        RenderToolBar(ui);

        if (bResized) bResized = false;

        auto mainViewportPos = ui->GetMainViewportPos();
        auto mainSize = ui->GetMainViewportSize();
        // 에디터 컨트롤러 상태 동기화
        if (m_editorController)
        {
            if (auto camera = m_editorController->GetPossessdCamera())
            {
                camera->SetAspect(m_viewportSize.x / m_viewportSize.y);
            }
            float clientViewportX = p0.x - mainViewportPos.x;
            float clientViewportY = p0.y - mainViewportPos.y;
            //m_editorController->SetViewportRect(p0.x, p0.y, m_viewportSize.x, m_viewportSize.y);
            m_editorController->SetViewportRect(clientViewportX, clientViewportY, m_viewportSize.x, m_viewportSize.y);
            m_editorController->SetViewportActive(m_bIsHovered, m_bIsFocused);
        }
    }
   
    // 뷰포트 좌측 하단에 Gizmo 렌더링
    RenderSceneGizmo(ui);

    // 뷰포트 메시지
    RenderScreenDebugMessages(ui);

    ui->EndPanel();
    ui->PopStyle_Var();
}

void ViewportPanel::SetEditorController(EditorController* controller)
{
    m_editorController = controller;
    m_cameraComponent = (m_editorController) ? m_editorController->GetPawn()->GetComponent<CameraComponent>() : nullptr;    
}

void ViewportPanel::RenderToolBar(IUIBuilder* ui)
{
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
    ui->BeginOverlay("PlayOverlay", toolOverlayPos, toolOverlaySize, 0.0f, toolOverlayFlags);

    ui->BeginDisabled(m_ownerApp->IsPlayMode());
    if (ui->Button("Play")) m_ownerApp->OnPlayButtonClicked();
    ui->EndDisabled();
    ui->SameLine();
    ui->BeginDisabled(!m_ownerApp->IsPlayMode());
    if (ui->Button("Stop")) m_ownerApp->OnStopButtonClicked();
    ui->EndDisabled();
    // HitProxy 디버깅을 위한 임시 코드
    ui->SameLine();
    if (ui->Button("Toggle HitProxy")) m_bHitProxyDebugMode = !m_bHitProxyDebugMode;

    ui->EndOverlay();

    if (m_editorController)
    {
        ui->SameLine();
        m_editorController->RenderGizmoOptionUI(ui);
    }
}

void ViewportPanel::RenderSceneGizmo(IUIBuilder* ui)
{
    if (m_viewportSize.x <= 0.0f || m_viewportSize.y <= 0.0f) return;
    float gizmoSize = 100.0f;
    UI::Vector<float, 2> gizmoPos = { 15.0f, 15.0f };
    UI::UI_PanelOption gizmoFlags = UI::UI_PanelOption::NoDecoration |
        UI::UI_PanelOption::NoInput |
        UI::UI_PanelOption::NoBackground |
        UI::UI_PanelOption::NoSavedSettings |
        UI::UI_PanelOption::NoFocusOnAppearing |
        UI::UI_PanelOption::NoNav;

    if (ui->BeginOverlay("Gizmo", gizmoPos, { gizmoSize, gizmoSize }, 0.0f, gizmoFlags, UI::UI_AlignmentX::Align_Left, UI::UI_AlignmentY::Align_Bottom))
    {
        // 중심점 계산
        UI::Vector<float, 2> center = ui->GetCursorScreenPos();
        center.x += gizmoSize * 0.5f;
        center.y += gizmoSize * 0.5f;
        float radius = 40.0f;
        struct Axis {
            XMVECTOR direction;
            UI::Color color;
            const char* label;
            float zDepth;
        };

        std::vector<Axis> axes = {
            { XMVectorSet(1, 0, 0, 0), {1.0f, 0.2f, 0.2f, 1.0f}, "X", 0.0f },
            { XMVectorSet(0, 1, 0, 0), {0.2f, 1.0f, 0.2f, 1.0f}, "Y", 0.0f },
            { XMVectorSet(0, 0, 1, 0), {0.2f, 0.2f, 1.0f, 1.0f}, "Z", 0.0f }
        };

        // 회전 계산
        if (m_cameraComponent)
        {
            XMMATRIX viewMat = m_cameraComponent->GetViewMatrix();
            for (auto& axis : axes)
            {
                XMVECTOR viewDir = XMVector3TransformNormal(axis.direction, viewMat);
                axis.direction = viewDir;
                axis.zDepth = XMVectorGetZ(viewDir);
            }
        }

        // Z-Sort (뒤에 있는 축부터 그리기 위해)
        std::sort(axes.begin(), axes.end(), [](const Axis& a, const Axis& b) {
            return a.zDepth > b.zDepth;
            });

        for (const auto& axis : axes)
        {
            float x = XMVectorGetX(axis.direction);
            float y = XMVectorGetY(axis.direction);
            UI::Vector<float, 2> endPos = { center.x + x * radius, center.y - y * radius };
            ui->DrawLine(center, endPos, axis.color, 3.0f); // 라인 그리기
            ui->DrawCircleFilled(endPos, 7.0f, axis.color); // 끝점 원 그리기

            // 텍스트 라벨 그리기 (중앙 정렬)
            UI::Vector<float, 2> textSize = ui->CalcTextSize(axis.label);
            UI::Vector<float, 2> textPos = {
                endPos.x - textSize.x * 0.5f,
                endPos.y - textSize.y * 0.5f
            };
            ui->DrawTextAt(textPos, { 1.0f, 1.0f, 1.0f, 1.0f }, axis.label);
        }
        ui->DrawCircleFilled(center, 4.0f, { 1.0f, 1.0f, 1.0f, 1.0f }); // Pivot
    }
    ui->EndOverlay();

    if (m_editorController) m_editorController->RenderGizmoUI(ui);

}

void ViewportPanel::RenderScreenDebugMessages(IUIBuilder* ui)
{
    if (m_viewportSize.x <= 0.0f || m_viewportSize.y <= 0.0f) return;
    // 텍스트 시작 위치 (뷰포트 영역의 좌측 상단 여백)
    auto vMin = ui->GetWindowContentMin();
    auto vPos = ui->GetWindowPos();
    UI::Vector<float, 2> textPos = { vPos.x + vMin.x + 15.0f, vPos.y + vMin.y + 15.0f };

    for (auto& message : m_onScreenMessages)
    {
        ui->DrawTextAt(textPos, message.color, message.text.c_str());
        textPos.y += 20.0f; // 다음 줄로 이동
    }
}

