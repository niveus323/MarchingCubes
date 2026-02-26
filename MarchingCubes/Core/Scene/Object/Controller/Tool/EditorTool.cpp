#include "pch.h"
#include "EditorTool.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Object/Controller/EditorController.h"

BEGIN_REFLECTION_ROOT(IEditorTool)
END_REFLECTION()

void IEditorTool::OnActivated(EditorController* controller)
{
	m_controller = controller;
	m_scene = m_controller->GetScene().get();
}

void IEditorTool::OnDeactivated()
{
	m_scene = nullptr;
	m_controller = nullptr;
}
