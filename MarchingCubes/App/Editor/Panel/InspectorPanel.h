#pragma once
#include "App/Editor/Interface/EditorPanel.h"
#include "Core/Engine/Reflection.h"

// Forward Declaration;
class GameObject;
class Component;
class TypeDescriptor;

class InspectorPanel : public IEditorPanel
{
public:
	InspectorPanel(EditorApp* app) : IEditorPanel(app) {}
	void OnRenderUI(IUIBuilder* ui) override;
	void SetTarget(GameObject* target) { m_target = target; }

private:
	void RenderComponentProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc);
	void DrawTypeProperties(IUIBuilder* ui, void* componentPtr, TypeDescriptor* typeDesc);
	void DrawSingleProperty(IUIBuilder* ui, void* instance, const Property& prop);

private:
	GameObject* m_target = nullptr;
};

