#include "pch.h"
#include "EditorPanel.h"
#include "App/Editor/Interface/EditorApp.h"

IEditorPanel::IEditorPanel(EditorApp* app) : 
	m_ownerApp(app)
{
}
