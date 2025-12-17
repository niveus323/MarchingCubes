#pragma once
#include "Win32Application.h"
#include "Core/Utils/Timer.h"
#include "Core/UI/UIRenderer.h"
#include "Core/Scene/Scene.h"
#include "Core/Trace/Profiler.h"
using Microsoft::WRL::ComPtr;

class ResourceManager;
class GpuAllocator;
class StaticBufferRegistry;
class UploadContext;
class DescriptorAllocator;
class RenderSystem;
class InputState;

class DXAppBase
{
public:
    DXAppBase(uint32_t width, uint32_t height, std::wstring name);
    virtual ~DXAppBase();

    void OnInit();
    virtual void OnDestroy();
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnUpdateUI(float deltaTime) {}
    void Render();

    void OnResize(uint32_t width, uint32_t height);
    void StartTimer();
    void TickAndUpdate();
    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);
    void OnPlatformEvent(uint32_t msg, WPARAM wParam, LPARAM lParam);
    // Accessors.
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    IUIRenderer* GetUIRenderer() const { return m_uiRenderer.get(); }
    ID3D12Resource* CurrentBackbuffer() const { return m_renderTargets[m_frameIndex].Get(); }

protected:
    virtual void InitUI(ID3D12GraphicsCommandList* cmd);
    virtual void OnUpload(ID3D12GraphicsCommandList* cmd) {};                           // 리소스 버퍼 할당
    virtual void RenderFrame(ID3D12GraphicsCommandList* cmd);
    virtual void OnAfterSwapchainCreated() {}                                           
    virtual void InitSubsystems();                                                      // 서브 시스템
    virtual void OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand) {}         // 초기 씬 구성
    virtual void OnAfterChainSwaped();

    virtual void CreateRootSignature() = 0;
    virtual void CreateInputElements() = 0;
    virtual std::unique_ptr<Scene> CreateDefaultScene() = 0;
    virtual std::vector<std::wstring> GetPSOFiles() const = 0;

    std::wstring GetAssetFullPath(LPCWSTR assetName) { return GetFullPath(AssetType::Default, assetName); }
	void GetHawrdwardAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHightPerformanceAdapter = false);
	void SetCustomWindowText(LPCWSTR text) const;

protected:
    Timer& GetTimer() { return m_timer; }
    const Timer& GetTimer() const { return m_timer; }
    ID3D12Device* GetDevice() { return m_device.Get(); }
    ID3D12CommandQueue* GetPresentQueue() const { return m_commandQueue.Get(); }
    ID3D12Fence* GetSwapChainFence() { return m_swapChainFence.Get(); }
    UINT64 GetSwapChainFenceValue() { return m_swapChainFence->GetCompletedValue(); }
    GpuAllocator* GetGpuAllocator() { return m_gpuAllocator.get(); }
    UploadContext* GetUploadContext() { return m_uploadContext.get(); }
    StaticBufferRegistry* GetStaticBufferRegistry() { return m_staticBufferRegistry.get(); }
    DescriptorAllocator* GetDescriptorAllocator() { return m_descriptorAllocator.get(); }
    ResourceManager* GetResourceManager() { return m_resourceManager.get(); }

private:
    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hwnd, ID3D12CommandQueue* presentQueue);
    void CreateCommandObjects();
    void InitPipeline();
    void InitializeScene();
    void LoadScene(std::unique_ptr<Scene> newScene);
    void CreateBackbuffersAndDefaultDSV(uint32_t width, uint32_t height);
    void DestroyBackbuffersAndDefaultDSV();
    void CreateFenceAndEvent();
    void DestroyFenceAndEvent();
    void PrepareRender();
    void MoveToNextFrame();
    void WaitForGpu();

    void InitGpuTimeStampResources();
    void DestroyGpuTimeStampResources();

    void GpuTimestampBegin(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex);
    void GpuTimestampEndAndResolve(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex);
    double ComputeGpuFrameMsAfterCompleted(uint32_t frameIndex); // fence 완료 후 호출

    static constexpr uint32_t QueryIndexStart(uint32_t frameIndex) { return frameIndex * 2; }

protected:
    static const uint32_t kFrameCount = 2;

    // Hardware
	uint32_t m_width;
	uint32_t m_height;
	float m_aspectRatio;
	bool m_userWarpDevice = false;
    BOOL m_tearingSupported = FALSE;

    // DirectX12 Common
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain; 
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[kFrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_dsvDescriptorSize = 0;

    // Pipeline
    ComPtr<ID3D12Resource> m_renderTargets[kFrameCount];
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElements;
    DXGI_FORMAT m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    // Fence
    ComPtr<ID3D12Fence> m_swapChainFence;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValues[kFrameCount];
    uint64_t m_nextFenceValue = 0;
    uint32_t m_frameIndex = 0u;

    // Memory
    std::unique_ptr<GpuAllocator> m_gpuAllocator;
    std::unique_ptr<UploadContext> m_uploadContext;
    std::unique_ptr<StaticBufferRegistry> m_staticBufferRegistry;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    // Subsystems
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<RenderSystem> m_renderSystem;
    std::unique_ptr<InputState> m_inputState;
    std::unique_ptr<IUIRenderer> m_uiRenderer;

    // Scene
    std::unique_ptr<Scene> m_currentScene;

    // GPU Timestamp Query
    ComPtr<ID3D12QueryHeap> m_tsQueryHeap;
    ComPtr<ID3D12Resource> m_tsReadback;     // READBACK buffer (uint64_t * 2 * kFrameCount)
    uint64_t* m_tsMapped = nullptr;
    uint64_t m_tsFreq = 0;     // ticks per second

    // Profiler
    std::weak_ptr<Profiler> m_profiler;
    std::shared_ptr<Profiler> m_profilerOwner; // TODO : ToolManager에서 소유하도록 변경

    UI::FrameCallbackToken m_uiToken_Fps = 0;
    UI::FrameCallbackToken m_uiToken_Profiler = 0;

    
private:
	std::wstring m_title;
    Timer m_timer;
};

