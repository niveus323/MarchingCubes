#include "pch.h"
#include "SceneHierarchyPanel.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/GameObject.h"
#include <algorithm>
#include <cctype>

void SceneHierarchyPanel::OnRenderUI(IUIBuilder* ui)
{
    ui->BeginPanel("Scene Hierarchy");

    if (m_currentScene)
    {
        ui->SearchBar("Search objects...", m_filterText);
        ui->Separator();

        if (ui->BeginTable("HierarchyTable", 1))
        {
            const auto& objects = m_currentScene->GetObjects();
            for (const auto& obj : objects)
            {
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
    ui->EndPanel();
}

void SceneHierarchyPanel::DrawNode(IUIBuilder* ui, GameObject* node, const std::string& filterText)
{
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
            m_selectedObject = node;
            // EditorApp에 알림이 필요하다면 여기서 처리
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
