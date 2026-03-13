#pragma once
#include "App/Common/DXAppBase.h"
#include "Core/Rendering/RenderSystem.h"
#include "App/Editor/Panel/SceneHierarchyPanel.h"
#include "App/Editor/Panel/InspectorPanel.h"
#include "../Panel/ViewportPanel.h"
#include <concepts>
using DebugViewModeHandle = int;

class EditorApp : public DXAppBase
{
public:
	EditorApp(uint32_t width, uint32_t height, std::wstring name);
	virtual ~EditorApp() = default;
	virtual void Destroy() override;
	virtual void Update(float deltaTime) override;
	virtual void UpdateUI(float deltaTime) override;

	D3D12_GPU_DESCRIPTOR_HANDLE GetOffscreenSRVGpuHandle();

	bool IsPlayMode() const { return m_currentScene? m_currentScene->IsPlaying() : false; }
	void OnPlayButtonClicked();
	void OnStopButtonClicked();
protected:
	virtual void InitUI(ID3D12GraphicsCommandList* cmd) override;
	virtual void RenderFrame(ID3D12GraphicsCommandList* cmd) override;
	void OnAfterSwapchainCreated() override;
	void OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand) override final;
	virtual void OnSceneLoaded(Scene* scene) override;
	virtual void UpdateInputCaptureState() override;
	virtual std::vector<std::wstring> GetPSOFiles() const override { return { L"EditorCommon.json" }; }

	// Debug View Mode
	DebugViewModeHandle RegisterDebugViewMode(std::string_view name, std::function<void(RenderSystem*)> func);
	void SetDebugViewMode(std::string_view name);
	void SetDebugViewMode(int index);
	DebugViewModeHandle GetCurrentDebugViewMode() { return m_currentDebugViewMode; }

private:
	void RenderFpsUI(IUIBuilder* ui);
	void RenderProfilingUI(IUIBuilder* ui);
	void RenderMainMenuBarUI(IUIBuilder* ui);
	void RenderSubsystemManagerUI(IUIBuilder* ui);
	void RequestResizeViewport(UI::Vector<float, 2> viewportSize);
	void OnResizeViewport(UI::Vector<float, 2> viewportSize);
	
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
	bool m_bInputCaptured = false;

	// Viewport
	ComPtr<ID3D12Resource> m_offscreenResource;
	ComPtr<ID3D12Resource> m_offscreenDepth;
	uint32_t m_offscreenRTVHandle = UINT32_MAX;
	uint32_t m_offscreenDSVHandle = UINT32_MAX;
	uint32_t m_offscreenSRVHandle = UINT32_MAX;
	bool m_bOffscreenHandlesAllocated = false;

	// DebugMode
	std::vector<std::pair<std::string, std::function<void(RenderSystem*)>>> m_debugViewModes;
	int m_currentDebugViewMode = 0;

	//UI
	ViewportPanel* m_viewportPanel = nullptr;
	UI::FrameCallbackToken m_uiToken_Viewport = 0;
	bool m_bResizePending = false;
	UI::Vector<float, 2> m_pendingViewportSize = { 0.0f, 0.0f };

	SceneHierarchyPanel* m_hierarchyPanel = nullptr;
	UI::FrameCallbackToken m_uiToken_Hierarchy = 0;

	InspectorPanel* m_inspectorPanel = nullptr;
	UI::FrameCallbackToken m_uiToken_Inspector = 0;
	
	UI::FrameCallbackToken m_uiToken_MainMenuBar = 0;
	bool m_bShowSubsystemManager = false;
	UI::FrameCallbackToken m_uiToken_SubsystemManager = 0;

	std::vector<std::unique_ptr<IEditorPanel>> m_editorPanels;
};

