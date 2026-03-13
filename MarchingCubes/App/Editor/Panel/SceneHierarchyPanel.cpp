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
            ui->SearchBar("Search objects...", m_filterText);
            ui->Separator();

            if (ui->IsWindowFocused() && ui->IsKeyPressed_Delete())
            {
                if (m_selectedObject)
                {
                    m_selectedObject->MarkForDestroy();
                    SetSelection(nullptr); // UI 즉시 초기화
                }
            }

            if (ui->BeginTable("HierarchyTable", 1))
            {
                for (const auto& obj : m_currentScene->GetObjects())
                {
                    // 부모가 있는 오브젝트는 DrawNode 내부에서 UI에 노출하므로 continue
                    if (obj->GetOwner() != nullptr) continue;
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
    // 검색어가 있고 이름에 포함 안 되면 스킵
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
        bool isLeaf = true;
        for (const auto& child : node->GetChildren())
        {
            if (!child->HasAnyFlags(EObjectFlags::Invisible))
            {
                isLeaf = false;
                break;
            }
        }

        //bool isLeaf = node->GetChildren().empty();
        bool isSelected = (m_selectedObject == node);
        bool opened = ui->BeginTreeNode(node->GetName().c_str(), isLeaf, isSelected);

        if (ui->IsItemClicked())
        {
            SetSelection(node);
        }

        if (ui->BeginPopupContextItem())
        {
            if (ui->MenuItem("Delete Object"))
            {
                node->MarkForDestroy();
                if (m_selectedObject == node) SetSelection(nullptr);
            }
            ui->EndPopup();
        }

        if (isSelected && ui->IsWindowFocused() && ui->IsKeyPressed_F12())
        {
            StartRenaming(node);
        }

        if (opened)
        {
            for (auto& child : node->GetChildren())
            {
                if (child->HasAnyFlags(EObjectFlags::Invisible)) continue;
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
