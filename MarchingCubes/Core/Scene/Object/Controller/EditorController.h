#pragma once
#include "Controller.h"
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"
#include <functional>

class EditorController : public Controller
{
	REFLECT_GENERATED_BODY(EditorController)
public:
	void Update(float deltaTime) override;
	void RenderUI(IUIBuilder* ui) override;

	void SetTool(std::shared_ptr<IEditorTool> newTool);
	std::shared_ptr<IEditorTool> GetActiveTool() const { return m_activeTool; }
	float GetCameraSpeed() const { return m_cameraSpeed; }
	void SetCameraSpeed(float newSpeed) { m_cameraSpeed = newSpeed; }
	void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
	GameObject* GetSelectedObject() { return m_selectedObject; }
	void SelectObject(GameObject* obj);
	void SetSelectionChangedCallback(std::function<void(GameObject*)> callback) { m_selectionCallback = callback; }
	void SetViewportActive(bool bHovered, bool bFocused);

protected:
	virtual void ProcessInput(float deltaTime) override;

private:
	void UpdateCameraMovement(float deltaTime);
	void RotateCamera(float deltaTime);
	void AddYawInput(float val);
	void AddPitchInput(float val);
	
private:
	std::shared_ptr<IEditorTool> m_activeTool; 
	float m_cameraSpeed = 50.0f;
	float m_mouseSensitivity = 0.01f;
	GameObject* m_selectedObject = nullptr;
	std::function<void(GameObject*)> m_selectionCallback;

	bool m_bViewportHovered = false;
	bool m_bViewportFocused = false;
};

