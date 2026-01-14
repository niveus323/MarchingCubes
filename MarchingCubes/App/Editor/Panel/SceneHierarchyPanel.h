#pragma once
#include "App/Editor/Interface/EditorPanel.h"

// Forward Declaration
class Scene;
class GameObject;

class SceneHierarchyPanel : public IEditorPanel
{
public:
	virtual void OnRenderUI(IUIBuilder* ui) override;
	void SetCurrentScene(Scene* scene) { m_currentScene = scene; }
	GameObject* GetSelectedObject() { return m_selectedObject; }

private:
	void DrawNode(IUIBuilder* ui, GameObject* node, const std::string& filterText);
	void StartRenaming(GameObject* target);
	bool ContainsIgnoreCase(const std::string& source, const std::string& target);

private:
	Scene* m_currentScene = nullptr;
	GameObject* m_selectedObject = nullptr;

	// 이름 변경
	GameObject* m_renamingObject = nullptr; 
	std::string m_renameBuffer = "";          
	bool m_isRenameFocusNeeded = false;     

	// 검색
	std::string m_filterText;
};

