#pragma once

class InputState;
class EditorController;
class Scene;
class IUIBuilder;
class GameObject;

class IEditorTool
{
	REFLECT_GENERATED_BODY(IEditorTool)
public:
	virtual void OnActivated(EditorController* controller);
	virtual void OnDeactivated();
	virtual void Update(float deltaTime) = 0;
	virtual bool ProcessInput(const InputState* input, float deltaTime) = 0;
	virtual void Render(ID3D12GraphicsCommandList* cmd) {}
	virtual void RenderUI(IUIBuilder* ui) {}
	virtual void OnSelectionUpdated(GameObject* selected) {}
protected:
	EditorController* m_controller = nullptr;
	Scene* m_scene = nullptr;
};

