#pragma once
#include "App/Editor/Interface/EditorPanel.h"

// Forward Declaration;
class GameObject;
class Component;
class TypeDescriptor;

class InspectorPanel : public IEditorPanel
{
public:
	void OnRenderUI(IUIBuilder* ui) override;

	void SetTarget(GameObject* target) { m_target = target; }

private:
	void RenderComponentProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc);
	void DrawTypeProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc);

private:
	GameObject* m_target = nullptr;
};

