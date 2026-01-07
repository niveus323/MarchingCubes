#pragma once

class InputState;
class EditorController;
class IUIBuilder;

class IEditorTool
{
public:
	virtual ~IEditorTool() = default;

	virtual void OnActivated(EditorController* controller) { m_owner = controller; }
	virtual void OnDeactivated() { m_owner = nullptr; }
	virtual void ProcessInput(const InputState* input, float deltaTime) = 0;
	virtual void Render(ID3D12GraphicsCommandList* cmd) {}
	virtual void RenderUI(IUIBuilder* ui) {}
protected:
	EditorController* m_owner = nullptr;
};

