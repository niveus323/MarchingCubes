#pragma once
#include "../Interface/EditorPanel.h"

//Forward Declaration
class CameraComponent;
class EditorController;

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorApp* app) : IEditorPanel(app) {}
	virtual void OnUpdate() override;
	virtual void OnRenderUI(IUIBuilder* ui) override;
	bool IsViewportFocused() const { return m_bIsFocused; }
	bool IsViewportHovered() const { return m_bIsHovered; }

	void SetOnViewportChanged(std::function<void(UI::Vector<float, 2>)> callback) { m_onViewportResized = callback; }
	UI::Vector<float, 2> GetViewportSize() const { return m_viewportSize; }
	void SetEditorController(EditorController* controller);

private:
	UI::Vector<float, 2> m_viewportSize = { 0.0f, 0.0f };
	UI::Vector<float, 2> m_viewportBounds[2]; // 마우스 피킹 연산을 위한 뷰포트 스크린 좌표

	bool m_bIsFocused = false;
	bool m_bIsHovered = false;

	std::function<void(UI::Vector<float, 2>)> m_onViewportResized;

	EditorController* m_editorController = nullptr; // 뷰포트 옵션으로 카메라 속도 등 변경을 위해 참조
	CameraComponent* m_cameraComponent = nullptr; // 씬 기즈모 렌더링을 위해 EditorController가 가진 카메라 참조
};

