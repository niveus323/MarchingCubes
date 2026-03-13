#include "pch.h"
#include "ContentBrowserPanel.h"

ContentBrowserPanel::ContentBrowserPanel(EditorApp* app) : IEditorPanel(app)
{
	m_baseDirectory = ASSETS_ROOT;
	m_currentDirectory = m_baseDirectory;
}

void ContentBrowserPanel::OnRenderUI(IUIBuilder* ui)
{
    if (!m_bShowPanel) return;

    if (ui->BeginPanel("Content Browser", &m_bShowPanel))
    {
        if (m_currentDirectory != m_baseDirectory)
        {
            if (ui->Button("<- Back", { 60.0f, 0.0f }))
            {
                m_currentDirectory = m_currentDirectory.parent_path();
            }
            ui->SameLine(0.0f, 4.0f);
        }

        ui->Text(m_currentDirectory.string());
        ui->Separator();

        float panelWidth = ui->GetAvailableWidth();
        int columnCount = static_cast<int>(panelWidth / iconSize);
        if (columnCount < 1) columnCount = 1;

        if (ui->BeginTable("ContentGrid", columnCount))
        {
            for (auto& directoryEntry : std::filesystem::directory_iterator(m_currentDirectory))
            {
                const auto& path = directoryEntry.path();
                std::string filenameString = path.filename().string();

                ui->TableNextColumn();
                ui->PushID(filenameString.c_str());

                // 임시 시각화: 폴더는 [D], 파일은 이름만 출력
                bool isDirectory = directoryEntry.is_directory();
                std::string buttonLabel = isDirectory ? std::format("[D] {}", filenameString) : filenameString;
                bool bIsSelected = (m_selectedItem == path);
                if (bIsSelected)
                {
                    UI::Color btnColor{ 0.2f, 0.4f, 0.8f, 1.0f };
                    UI::Color hoverColor{ 0.25f, 0.5f, 1.0f, 1.0f };
                    UI::Color activeColor{ 0.15f, 0.3f, 0.6f, 1.0f };
                    ui->PushStyle_Button(btnColor, hoverColor, activeColor);
                }
                bool bSingleClicked = ui->Button(buttonLabel.c_str(), { iconSize - 10.0f, 40.0f });
                if (bIsSelected)
                {
                    ui->PopStyle_Color(3);
                }

                if (ui->IsItemDoubleClicked())
                {
                    if (isDirectory) m_currentDirectory /= path.filename();
                }
                else if (bSingleClicked)
                {
                    m_selectedItem = path;
                }
                
                // --- 드래그 앤 드롭 연동 ---
                if (!isDirectory && ui->BeginDragDropSource())
                {
                    std::string itemPath = path.lexically_relative(ASSETS_ROOT).string();
                    std::string extension = path.extension().string();

                    // 메타데이터 기반 필터링을 위한 Payload 타입 세분화
                    std::string payloadType = "ITEM_Data";
                    if (extension == ".FBX" || extension == ".obj") payloadType = "ITEM_Mesh";
                    else if (extension == ".png" || extension == ".jpg") payloadType = "ITEM_Texture";
                    else if (extension == ".json") payloadType = "ITEM_Material"; //TODO : .json이 아닌 .mat파일로 변경

                    ui->SetDragDropPayload(payloadType.c_str(), itemPath.c_str(), itemPath.size() + 1);
                    ui->Text(filenameString); // 드래그 중 마우스 커서에 띄울 텍스트
                    Log::Print("ContentBrowserPanel", "payloadType : %s", payloadType.c_str());
                    ui->EndDragDropSource();
                }

                ui->PopID();
            }
            ui->EndTable();
        }
    }
    ui->EndPanel();
}
