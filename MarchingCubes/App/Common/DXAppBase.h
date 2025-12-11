#pragma once
#include "Win32Application.h"
#include "Core/Utils/Timer.h"
#include "Core/UI/UIRenderer.h"
using Microsoft::WRL::ComPtr;

class ResourceManager;
class GpuAllocator;
class StaticBufferRegistry;
class UploadContext;
class DescriptorAllocator;

class DXAppBase
{
public:
    DXAppBase(uint32_t width, uint32_t height, std::wstring name);
    virtual ~DXAppBase();

    void OnInit();
    virtual void OnDestroy();
    virtual void OnUpdate(float deltaTime) = 0;
    void Render();

    void OnResize(uint32_t width, uint32_t height);
    void StartTimer();
    void TickAndUpdate();
    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    virtual void OnPlatformEvent(uint32_t msg, WPARAM wParam, LPARAM lParam) = 0;
    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(WPARAM key) {}
    virtual void OnKeyUp(WPARAM key) {}
    virtual void OnMouseMove(int xPos, int yPos, WPARAM buttonState) {}
    virtual void OnMouseBtnDown(int x, int y, WPARAM button) {}
    virtual void OnMouseBtnUp(int x, int y, WPARAM button) {}

    // Accessors.
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    IUIRenderer* GetUIRenderer() const { return m_uiRenderer.get(); }
    ID3D12Resource* CurrentBackbuffer() const { return m_renderTargets[m_frameIndex].Get(); }

protected:
    // 파생 클래스가 선택적으로 구현
    virtual void OnUpload(ID3D12GraphicsCommandList* cmd) {};                           // 리소스 버퍼 할당
    virtual void OnRender() {};                                                         // 프레임 렌더링
    virtual void OnAfterSwapchainCreated() {}                                           // 오프스크린/힙/업로드/프레임 자원
    virtual void OnInitSubSystems() {}                                                  // 서브 시스템
    virtual void OnInitPipelines() {}                                                   // 공통 루트시그니처/PSO
    virtual void OnBuildInitialScene(ID3D12GraphicsCommandList* initCommand) {}         // 초기 씬 구성
    virtual void OnAfterChainSwaped() {}

	std::wstring GetAssetFullPath(LPCWSTR assetName);
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
    void CreateBackbuffersAndDefaultDSV(uint32_t width, uint32_t height);
    void CreateSubsystems();
    void InitializeScene();
    void DestroyBackbuffersAndDefaultDSV();
    void CreateFenceAndEvent();
    void DestroyFenceAndEvent();
    void PrepareRender();
    void MoveToNextFrame();
    void WaitForGpu();

protected:
    static const uint32_t kFrameCount = 2;
	uint32_t m_width;
	uint32_t m_height;
	float m_aspectRatio;
	bool m_userWarpDevice = false;

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

    ComPtr<ID3D12Resource> m_renderTargets[kFrameCount];
    ComPtr<ID3D12Resource> m_depthStencil;

    DXGI_FORMAT m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    ComPtr<ID3D12Fence> m_swapChainFence;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValues[kFrameCount];
    uint64_t m_nextFenceValue = 0;
    uint32_t m_frameIndex = 0u;

    BOOL m_tearingSupported = FALSE;

    // UI
    std::unique_ptr<IUIRenderer> m_uiRenderer;

private:
	std::wstring m_title;
    Timer m_timer;

    // Memory
    std::unique_ptr<GpuAllocator> m_gpuAllocator;
    std::unique_ptr<UploadContext> m_uploadContext;
    std::unique_ptr<StaticBufferRegistry> m_staticBufferRegistry;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    //Subsystems
    std::unique_ptr<ResourceManager> m_resourceManager;
};

