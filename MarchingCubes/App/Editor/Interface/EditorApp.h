#pragma once
#include "App/Common/DXAppBase.h"
#include "Core/Rendering/RenderSystem.h"
#include "Contents/Scene/Scene_Terraform.h"
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
	void RenderFpsUI();
	void RenderProfilingUI();

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
};

