#pragma once
#include "App/Common/DXAppBase.h"
#include "Core/Rendering/Camera.h"
#include "Core/Rendering/LightManager.h"
#include "Core/Geometry/Mesh/Mesh.h"
#include "Core/Input/InputState.h"
#include "Core/Rendering/RenderSystem.h"
#include "Core/Utils/Profiler.h"

class EditorApp : public DXAppBase
{
public:
	EditorApp(UINT width, UINT height, std::wstring name) : 
		DXAppBase(width, height, name), 
		m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 
		m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)) 
	{};
	virtual ~EditorApp() = default;

	// DXAppBase을(를) 통해 상속됨
	virtual void OnDestroy() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRender() override;

	void OnPlatformEvent(UINT msg, WPARAM wParam, LPARAM lParam) override;
protected:
	virtual void CreateQueues();
	virtual ID3D12CommandQueue* GetPresentQueue() const { return m_commandQueue.Get(); };

	virtual void OnAfterSwapchainCreated() override;
	virtual void OnInitPipelines() override;
	ID3D12GraphicsCommandList* BeginInitCommand();
	void OnBuildInitialScene() override final;
	void EndInitCommand();

	virtual void InitScene(ID3D12GraphicsCommandList* cmd) {}
	virtual void InitUICommon(ID3D12GraphicsCommandList* cmd);
	virtual void InitUI();
	virtual void UpdateScene(float deltaTime) {}
	virtual void UpdateUI(float deltaTime);
	// 렌더링 명령을 추가 전 필요한 작업은 이쪽으로 (ex 버퍼 업로드)
	virtual void BeforeDraw(ID3D12GraphicsCommandList* cmd);
	virtual void DrawScene(ID3D12GraphicsCommandList* cmd);
	// 렌더링 명령 추가 후 필요한 작업은 이쪽으로 (ex PostProcessing)
	virtual void AfterDraw(ID3D12GraphicsCommandList* cmd) {}

	void MoveToNextFrame();
	void WaitForGpu();

	virtual std::vector<std::wstring> GetPSOFiles() const { return { L"EditorCommon.json" }; }
private:
	void InitGpuTimeStampResources();
	void DestroyGpuTimeStampResources();

	void GpuTimestampBegin(ID3D12GraphicsCommandList * cmd, UINT frameIndex);
	void GpuTimestampEndAndResolve(ID3D12GraphicsCommandList * cmd, UINT frameIndex);
	double ComputeGpuFrameMsAfterCompleted(UINT frameIndex); // fence 완료 후 호출

	static constexpr UINT QueryIndexStart(UINT frameIndex) { return frameIndex * 2; }

	// UI
	void RenderFpsUI();
	void RenderProfilingUI();

protected:
	// Input
	InputState m_inputState;

	// Render
	// TODO : PBR 테스트를 위해 임시로 Material을 app에서 초기화하여 사용, ResoureManager 및 Wrapper 클래스로 만들어서 .mat 파일 로드하여 인스턴스를 wrapper 클래스에 넘기는 방식으로 수정할 것.
	std::vector<ComPtr<ID3D12Resource>> m_toDeletesContainer;
	std::shared_ptr<Material> m_defaultMat;
	std::unique_ptr<Camera> m_mainCamera;
	std::unique_ptr<LightManager> m_lightManager;
	std::vector<std::unique_ptr<IDrawable>> m_StaticObjects;

	std::unique_ptr<RenderSystem> m_renderSystem;

	// debug
#ifdef _DEBUG
	bool m_debugViewEnabled = false;
	bool m_wireFrameEnabled = false;
	bool m_profileingEnabled = true;
#endif // _DEBUG

private:
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[kFrameCount];
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	std::vector<PendingDeleteItem> m_pendingDeletes;

	ComPtr<ID3D12RootSignature> m_rootSignature;

	// GPU Timestamp Query
	ComPtr<ID3D12QueryHeap> m_tsQueryHeap;
	ComPtr<ID3D12Resource> m_tsReadback;     // READBACK buffer (UINT64 * 2 * kFrameCount)
	UINT64* m_tsMapped = nullptr;
	UINT64 m_tsFreq = 0;     // ticks per second

	std::weak_ptr<Profiler> m_profiler;
	std::shared_ptr<Profiler> m_profilerOwner; // TODO : ToolManager에서 소유하도록 변경

	UI::FrameCallbackToken m_uiToken_Fps = 0;
	UI::FrameCallbackToken m_uiToken_Profiler = 0;
};

