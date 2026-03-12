#pragma once
#include "Controller.h"
#include "Core/Scene/Object/Controller/Tool/EditorTool.h"
#include "Core/UI/Builder/UIBuilder.h"
#include "Core/Math/PhysicsHelper.h"
#include <functional>

class EditorController : public Controller
{
	REFLECT_GENERATED_BODY(EditorController)
public:
	void Update(float deltaTime) override;
	void RenderUI(IUIBuilder* ui) override;

	// --- Editor Tool ---
	std::shared_ptr<IEditorTool> GetActiveTool() const { return m_activeTool; }
	void SetTool(std::shared_ptr<IEditorTool> newTool);

	// --- Camera ---
	float GetCameraSpeed() const { return m_cameraSpeed; }
	void SetCameraSpeed(float newSpeed) { m_cameraSpeed = newSpeed; }

	// --- Object Picking ---
	GameObject* GetSelectedObject() { return m_selectedObject; }
	void SelectObject(GameObject* obj);
	void SetSelectionChangedCallback(std::function<void(GameObject*)> callback) { m_selectionCallback = callback; }
	void SetViewportActive(bool bHovered, bool bFocused);

	// --- Gizmo ---
	void RenderGizmoOptionUI(IUIBuilder* ui);
	void RenderGizmoUI(IUIBuilder* ui);
	float GetGizmoSize() const { return m_gizmoSize; }
	void SetGizmoSize(float size) { m_gizmoSize = size; }

	// --- Viewport ---
	void SetViewportRect(float x, float y, float width, float height)
	{
		m_viewportX = x;
		m_viewportY = y;
		m_viewportWidth = width;
		m_viewportHeight = height;
	}
	float GetViewportWidth() const { return m_viewportWidth; }
	float GetViewportHeight() const { return m_viewportHeight; }
	
	PhysicsUtil::Ray GetViewportMouseRay();

protected:
	virtual void ProcessInput(float deltaTime) override;

private:
	void UpdateCameraMovement(float deltaTime);
	void RotateCamera(float deltaTime);
	void AddYawInput(float val);
	void AddPitchInput(float val);
	//GameObject* PerformMousePicking(float mouseX, float mouseY);
	uint32_t DecodeHitProxyID(const DirectX::XMUINT4& pixel);
	
private:
	std::shared_ptr<IEditorTool> m_activeTool; 
	float m_cameraSpeed = 50.0f;
	float m_mouseSensitivity = 0.01f;
	float m_gizmoSize = 0.1f;
	GameObject* m_selectedObject = nullptr;
	std::function<void(GameObject*)> m_selectionCallback;

	// --- Viewport ---
	float m_viewportX = 0.0f;
	float m_viewportY = 0.0f;
	float m_viewportWidth = 1280.0f;
	float m_viewportHeight = 720.0f;

	// --- Flags ---
	bool m_bViewportHovered = false;
	bool m_bViewportFocused = false;
	bool m_bIsGizmoHovered = false;
	bool m_bUseSnap = false;

	// --- Gizmo ---
	UI::EGizmoOperation m_currentGizmoOperation = UI::EGizmoOperation::Translate;
	UI::EGizmoMode m_currentGizmoMode = UI::EGizmoMode::Local;
	float m_snapTranslation = 10.0f;
	float m_snapRotation = 10.0f; // Degree
	float m_snapScale = 0.5f;

	// --- Mouse Picking ---
	uint64_t m_pickingFenceValue = 0;
};

