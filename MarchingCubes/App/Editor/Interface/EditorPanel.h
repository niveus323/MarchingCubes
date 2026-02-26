#pragma once
#include "Core/UI/Builder/UIBuilder.h"

class EditorApp;

class IEditorPanel
{
public:
	IEditorPanel(EditorApp* app);
	virtual ~IEditorPanel() = default;
	virtual void OnUpdate() {};
	virtual void OnRenderUI(IUIBuilder* ui) = 0;
	virtual bool IsPanelVisible() const { return m_bShowPanel; }
	virtual void SetPanelVisible(bool bShow) { m_bShowPanel = bShow; }
protected:
	EditorApp* m_ownerApp = nullptr;
	bool m_bShowPanel = true;
};

