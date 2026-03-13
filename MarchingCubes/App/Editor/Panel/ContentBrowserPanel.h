#pragma once
#include "App/Editor/Interface/EditorPanel.h"

class ContentBrowserPanel : public IEditorPanel
{
public:
	ContentBrowserPanel(EditorApp* app);

	virtual void OnRenderUI(IUIBuilder* ui) override;
private:
	std::filesystem::path m_baseDirectory;
	std::filesystem::path m_currentDirectory;
	std::filesystem::path m_selectedItem;
	float iconSize = 100.0f;
};

