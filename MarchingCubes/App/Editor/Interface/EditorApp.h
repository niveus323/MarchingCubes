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
	EditorApp(uint32_t width, uint32_t height, std::wstring name) :
		DXAppBase(width, height, name),
		m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
		m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
	{
	};
	virtual ~EditorApp() = default;
	virtual void OnDestroy() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRender() override;

	void OnPlatformEvent(uint32_t msg, WPARAM wParam, LPARAM lParam) override;
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
	virtual void SyncGpu(ID3D12GraphicsCommandList* cmd) {};  // NOTE : ResourceSystem/DebugDrawSystem 추가 시 해당 내용을 옮긴다.
	virtual void DrawScene(ID3D12GraphicsCommandList* cmd);
	// 렌더링 명령 추가 후 필요한 작업은 이쪽으로 (ex PostProcessing)
	virtual void AfterDraw(ID3D12GraphicsCommandList* cmd) {}

	void MoveToNextFrame();
	void WaitForGpu();

	virtual std::vector<std::wstring> GetPSOFiles() const { return { L"EditorCommon.json" }; }
private:
	void InitGpuTimeStampResources();
	void DestroyGpuTimeStampResources();

	void GpuTimestampBegin(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex);
	void GpuTimestampEndAndResolve(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex);
	double ComputeGpuFrameMsAfterCompleted(uint32_t frameIndex); // fence 완료 후 호출

	static constexpr uint32_t QueryIndexStart(uint32_t frameIndex) { return frameIndex * 2; }

	// UI
	void RenderFpsUI();
	void RenderProfilingUI();

protected:
	// Input
	InputState m_inputState;

	// Render
	std::unique_ptr<Camera> m_mainCamera;
	std::unique_ptr<LightManager> m_lightManager;
	std::unique_ptr<RenderSystem> m_renderSystem;

	// Memory
	std::unique_ptr<GpuAllocator> m_gpuAllocator;
	std::unique_ptr<UploadContext> m_uploadContext;
	std::unique_ptr<StaticBufferRegistry> m_staticBufferRegistry;
	std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

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
	ComPtr<ID3D12RootSignature> m_rootSignature;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	// GPU Timestamp Query
	ComPtr<ID3D12QueryHeap> m_tsQueryHeap;
	ComPtr<ID3D12Resource> m_tsReadback;     // READBACK buffer (uint64_t * 2 * kFrameCount)
	uint64_t* m_tsMapped = nullptr;
	uint64_t m_tsFreq = 0;     // ticks per second

	std::weak_ptr<Profiler> m_profiler;
	std::shared_ptr<Profiler> m_profilerOwner; // TODO : ToolManager에서 소유하도록 변경

	UI::FrameCallbackToken m_uiToken_Fps = 0;
	UI::FrameCallbackToken m_uiToken_Profiler = 0;

	// Material
	// NOTE : ResourceManager 구현 전에 임시 사용을 위한 Material StructuredBuffer
	std::vector<MaterialGPU> m_materials;
	ComPtr<ID3D12Resource> m_materialBufferTable;
	uint32_t m_materialSlot = UINT32_MAX;
};

