#pragma once
#include "../Interface/EditorPanel.h"

//Forward Declaration
class CameraComponent;
class EditorController;

struct OnScreenMessage
{
	std::string text;
	float timeRemaining;
	UI::Color color;
};

class ViewportPanel : public IEditorPanel
{
public:
	ViewportPanel(EditorApp* app);
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRenderUI(IUIBuilder* ui) override;

	UI::Vector<float, 2> GetViewportSize() const { return m_viewportSize; }
	void SetOnViewportChanged(std::function<void(UI::Vector<float, 2>)> callback) { m_onViewportResized = callback; }
	void SetEditorController(EditorController* controller);
	void AddOnScreenDebugMessage(const std::string& msg, float timeToDisplay = 2.0f, UI::Color color = { 1.0f, 1.0f, 1.0f, 1.0f }) { m_onScreenMessages.push_back({ msg, timeToDisplay, color }); }
private:
	void RenderToolBar(IUIBuilder* ui);
	void RenderSceneGizmo(IUIBuilder* ui);
	void RenderScreenDebugMessages(IUIBuilder* ui);

private:
	UI::Vector<float, 2> m_viewportSize = { 0.0f, 0.0f };

	std::function<void(UI::Vector<float, 2>)> m_onViewportResized;
	std::vector<OnScreenMessage> m_onScreenMessages;

	EditorController* m_editorController = nullptr; // 뷰포트 옵션으로 카메라 속도 등 변경을 위해 참조
	CameraComponent* m_cameraComponent = nullptr; // 씬 기즈모 렌더링을 위해 EditorController가 가진 카메라 참조

	// Debug
	bool m_bHitProxyDebugMode = false;
};

