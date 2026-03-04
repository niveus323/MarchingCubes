#include "pch.h"
#include "ViewportPanel.h"
#include "../Interface/EditorApp.h"
#include "Core/Scene/Object/Controller/EditorController.h"
#include "Core/Scene/Object/SpectatorPawn.h"
#include "Core/Scene/Component/CameraComponent.h"

void ViewportPanel::OnUpdate()
{
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

        UI::Vector<float, 2> viewportPanelSize = ui->GetRegionAvailable();
        if (m_viewportSize.x != viewportPanelSize.x || m_viewportSize.y != viewportPanelSize.y)
        {
            bResized = true;
            m_viewportSize = viewportPanelSize;
            if (m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)  m_onViewportResized(m_viewportSize);
        }
        
        uint64_t srvGPUHandle = m_ownerApp->GetOffscreenSRVGpuHandle().ptr;
        
        if (srvGPUHandle != 0 && m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)
        {
            void* textureID = reinterpret_cast<void*>(srvGPUHandle);
            ui->Image(textureID, m_viewportSize);
        }
        else
        {
            auto p0 = ui->GetCursorScreenPos();
            ui->Dummy(m_viewportSize);
            ui->DrawRectFilled(p0, { p0.x + m_viewportSize.x, p0.y + m_viewportSize.y }, { 0.1f, 0.1f, 0.1f, 1.0f });
        }

        if (m_editorController) m_editorController->RenderGizmoUI(ui);

        auto vMin = ui->GetWindowContentMin();
        auto vMax = ui->GetWindowContentMax();
        auto vPos = ui->GetWindowPos();
        m_viewportBounds[0] = { vMin.x + vPos.x, vMin.y + vPos.y };
        m_viewportBounds[1] = { vMax.x + vPos.x, vMax.y + vPos.y };

        if (bResized) bResized = false;
    }
    // 에디터 컨트롤러 상태 동기화
    if (m_editorController) m_editorController->SetViewportActive(m_bIsHovered, m_bIsFocused);
    ui->EndPanel();
    ui->PopStyle();

    // 뷰포트 좌측 하단에 Gizmo 렌더링
    if (m_viewportSize.x <= 0.0f || m_viewportSize.y <= 0.0f) return;
    float gizmoSize = 100.0f;
    UI::Vector<float, 2> gizmoPos = {
        m_viewportBounds[0].x + 15.0f,
        m_viewportBounds[1].y - gizmoSize - 15.0f
    };
    if (ui->BeginOverlay("Gizmo", gizmoPos, { gizmoSize, gizmoSize }))
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
    ui->EndOverlay(); // 오버레이 종료
}

void ViewportPanel::SetEditorController(EditorController* controller)
{
    m_editorController = controller;
    m_cameraComponent = m_editorController->GetPawn()->GetComponent<CameraComponent>();
}

