#pragma once
#include "App/Common/DXAppBase.h"
#include "Core/Rendering/RenderSystem.h"
#include "Contents/Scene/Terraform/Scene_Terraform.h"
#include "App/Editor/Panel/SceneHierarchyPanel.h"
#include "App/Editor/Panel/InspectorPanel.h"
using DebugViewModeHandle = int;

// Forward Declaration
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
	virtual void CreateRootSignature() override;
	virtual void CreateInputElements() override;
	virtual std::unique_ptr<Scene> CreateDefaultScene() override { return std::make_unique<Scene_Terraform>(); }
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
	void OnPlayButtonClicked();
	void OnCloseButtonClicked();

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

	// TODO : 에디터에 여러 Panel을 관리할 수 있도록 컨테이너로 관리 + Add Panel 같은 함수 추가로 엔진 상단 옵션 탭에서 View 옵션에 선택으로 옵션 제공.
	std::unique_ptr<SceneHierarchyPanel> m_hierarchyPanel;
	UI::FrameCallbackToken m_uiToken_Hierarchy = 0;

	std::unique_ptr<InspectorPanel> m_inspectorPanel;
	UI::FrameCallbackToken m_uiToken_Inspector = 0;

	GameObject* m_selectedObject = nullptr;
};

