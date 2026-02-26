#include "pch.h"
#include "SceneHierarchyPanel.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/GameObject.h"
#include <algorithm>
#include <cctype>

void SceneHierarchyPanel::OnRenderUI(IUIBuilder* ui)
{
    if (!m_bShowPanel) return;

    if (ui->BeginPanel("Scene Hierarchy", &m_bShowPanel))
    {
        if (m_currentScene)
        {
            // TODO : 오브젝트 추가 버튼 
            //if(ui->Button("Add Object", { 100, 50 }))

            ui->SearchBar("Search objects...", m_filterText);
            ui->Separator();

            if (ui->BeginTable("HierarchyTable", 1))
            {
                const auto& objects = m_currentScene->GetObjects();
                for (const auto& obj : objects)
                {
                    if (obj->HasAnyFlags(EObjectFlags::Invisible)) continue;
                    if (!m_filterText.empty())
                    {
                        if (!ContainsIgnoreCase(obj->GetName(), m_filterText)) continue;
                    }

                    DrawNode(ui, obj.get(), m_filterText);
                }
                ui->EndTable();
            }

            if (ui->IsItemClicked() && ui->IsAnyItemHovered())
            {
                m_selectedObject = nullptr;
                m_renamingObject = nullptr;
            }
        }
    }
    ui->EndPanel();
}

void SceneHierarchyPanel::SetSelection(GameObject* selected)
{
    m_selectedObject = selected;
    m_onSelectionChanged(m_selectedObject);
}

void SceneHierarchyPanel::DrawNode(IUIBuilder* ui, GameObject* node, const std::string& filterText)
{
    // Flag 체크 (굳이 Panel에 보여줄 필요 없는 디버깅 목적 등의 객체는 패스)
    if (node->HasAnyFlags(EObjectFlags::Invisible)) return;
    // 검색어가 있고 이름에 포함 안 되면 스킵
    // (실제로는 자식이 포함되면 부모도 보여줘야 하므로 로직이 더 복잡할 수 있음)
    if (!filterText.empty() && !ContainsIgnoreCase(node->GetName(), filterText)) return;

    ui->PushID(node);
    if (m_renamingObject == node)
    {
        // F12 누른 직후라면 포커스 강제 설정
        if (m_isRenameFocusNeeded)
        {
            ui->SetKeyboardFocus();
            m_isRenameFocusNeeded = false;
        }

        bool enterPressed = ui->InputText("##Rename", m_renameBuffer);

        if (enterPressed || ui->IsItemDeactivated())
        {
            if (!m_renameBuffer.empty())
            {
                node->SetName(m_renameBuffer);
            }
            m_renamingObject = nullptr;
        }
    }
    else
    {
        bool isLeaf = node->GetChildren().empty(); // && node->GetComponents().empty()
        bool isSelected = (m_selectedObject == node);
        bool opened = ui->BeginTreeNode(node->GetName().c_str(), isLeaf, isSelected);

        if (ui->IsItemClicked())
        {
            SetSelection(node);
        }

        if (isSelected && ui->IsWindowFocused() && ui->IsKeyPressed_F12())
        {
            StartRenaming(node);
        }

        if (opened)
        {
            for (auto& child : node->GetChildren())
            {
                DrawNode(ui, child.get(), filterText);
            }
            ui->EndTreeNode();
        }
    }

    ui->PopID();
}

void SceneHierarchyPanel::StartRenaming(GameObject* target)
{
    m_renamingObject = target;
    m_isRenameFocusNeeded = true;
    m_renameBuffer = target->GetName();
}

bool SceneHierarchyPanel::ContainsIgnoreCase(const std::string& source, const std::string& target)
{
    auto it = std::search(
        source.begin(), source.end(),
        target.begin(), target.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != source.end());
}
