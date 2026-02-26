#pragma once
#include "App/Common/DXAppBase.h"
#include "Core/Rendering/RenderSystem.h"
#include "Contents/Scene/Terraform/Scene_Terraform.h"
#include "App/Editor/Panel/SceneHierarchyPanel.h"
#include "App/Editor/Panel/InspectorPanel.h"
using DebugViewModeHandle = int;

class EditorApp : public DXAppBase
{
public:
	EditorApp(uint32_t width, uint32_t height, std::wstring name) :
		DXAppBase(width, height, name)
	{ 
	}
	virtual ~EditorApp() = default;
	virtual void OnDestroy() override;
	virtual void OnUpdate(float deltaTime) override;
protected:
	void OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand) override final;

	virtual void InitUI(ID3D12GraphicsCommandList* cmd) override;
	virtual void OnUpdateUI(float deltaTime) override;
	virtual void OnSceneLoaded(Scene* scene) override;
	virtual void CreateInputElements() override;
	virtual std::shared_ptr<Scene> CreateDefaultScene() override { return std::make_shared<Scene_Terraform>(); }
	virtual std::vector<std::wstring> GetPSOFiles() const override { return { L"EditorCommon.json" }; }

	// Debug View Mode
	DebugViewModeHandle RegisterDebugViewMode(std::string_view name, std::function<void(RenderSystem*)> func);
	void SetDebugViewMode(std::string_view name);
	void SetDebugViewMode(int index);
	DebugViewModeHandle GetCurrentDebugViewMode() { return m_currentDebugViewMode; }

private:
	void RenderFpsUI(IUIBuilder* ui);
	void RenderHierarchyUI(IUIBuilder* ui);
	void RenderInspectorUI(IUIBuilder* ui);
	void RenderProfilingUI(IUIBuilder* ui);
	void RenderMainMenuBarUI(IUIBuilder* ui);
	void RenderSubsystemManagerUI(IUIBuilder* ui);
	void OnPlayButtonClicked();
	void OnCloseButtonClicked();
	
	template<std::derived_from<IEditorPanel> T, typename... Args>
	T* AddPanel(Args&&...args)
	{
		T* ptr = GetPanel<T>();
		if (ptr) return ptr; //이미 생성해둔게 있을 경우 Return

		auto newPanel = std::make_unique<T>(this, std::forward<Args>(args)...);
		ptr = newPanel.get();
		m_editorPanels.push_back(std::move(newPanel));
		return ptr;
	}

	template<std::derived_from<IEditorPanel> T>
	T* GetPanel()
	{
		for (auto& panel : m_editorPanels)
		{
			if (T* typed = dynamic_cast<T*>(panel.get()))
				return typed;
		}
		return nullptr;
	}

protected:
	// Debug
#ifdef _DEBUG
	bool m_profileingEnabled = true;
#endif // _DEBUG
	DebugViewModeHandle m_hDefaultView = -1;
	DebugViewModeHandle m_hWireView = -1;
	DebugViewModeHandle m_hNormalView = -1;

private:
	std::vector<std::pair<std::string, std::function<void(RenderSystem*)>>> m_debugViewModes;
	int m_currentDebugViewMode = 0;

	SceneHierarchyPanel* m_hierarchyPanel = nullptr;
	UI::FrameCallbackToken m_uiToken_Hierarchy = 0;

	InspectorPanel* m_inspectorPanel = nullptr;
	UI::FrameCallbackToken m_uiToken_Inspector = 0;
	
	UI::FrameCallbackToken m_uiToken_MainMenuBar = 0;
	bool m_bShowSubsystemManager = false;
	UI::FrameCallbackToken m_uiToken_SubsystemManager = 0;

	std::vector<std::unique_ptr<IEditorPanel>> m_editorPanels;
};

