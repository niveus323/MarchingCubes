#pragma once
#include "Controller.h"
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"

class EditorController : public Controller
{
	REFLECT_GENERATED_BODY()
public:
	EditorController(Scene* scene);
	virtual ~EditorController() = default;
	void RenderUI(IUIBuilder* ui) override;

	void SetTool(std::shared_ptr<IEditorTool> newTool);
	float GetCameraSpeed() const { return m_cameraSpeed; }
	void SetCameraSpeed(float newSpeed) { m_cameraSpeed = newSpeed; }
	void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

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

};

