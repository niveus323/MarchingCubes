#pragma once
#include "Core/UI/Builder/UIBuilder.h"

class EditorApp;

class IEditorPanel
{
public:
	IEditorPanel(EditorApp* app);
	virtual ~IEditorPanel() = default;
	virtual void OnUpdate(float deltaTime) {};
	virtual void OnRenderUI(IUIBuilder* ui) = 0;
	virtual bool IsPanelVisible() const { return m_bShowPanel; }
	virtual void SetPanelVisible(bool bShow) { m_bShowPanel = bShow; }
	virtual bool IsAllowingMouse() { return m_bShowPanel && m_bAllowEngineInput && IsHovered(); }
	virtual bool IsAllowingKeyboard() { return m_bShowPanel && m_bAllowEngineInput && IsFocused(); }

	bool IsHovered() const { return m_bIsHovered; }
	bool IsFocused() const { return m_bIsFocused; }

protected:
	EditorApp* m_ownerApp = nullptr;
	bool m_bShowPanel = true;
	bool m_bAllowEngineInput = false;
	bool m_bIsFocused = false;
	bool m_bIsHovered = false;
};

