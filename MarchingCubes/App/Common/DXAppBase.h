#pragma once
#include "Win32Application.h"
#include "Core/Utils/Timer.h"
#include "Core/UI/UIRenderer.h"
using Microsoft::WRL::ComPtr;

class DXAppBase
{
public:
    DXAppBase(UINT width, UINT height, std::wstring name) :
        m_width(width),
        m_height(height),
        m_title(name)
    {
        m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        std::fill(std::begin(m_fenceValues), std::end(m_fenceValues), 0ull);
    }
    virtual ~DXAppBase() = default;

    void OnInit(); 
    virtual void OnDestroy();
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnRender() = 0;

    void OnResize(UINT width, UINT height);
    void StartTimer();
    void TickAndUpdate();
    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    virtual void OnPlatformEvent(UINT msg, WPARAM wParam, LPARAM lParam) = 0;
    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(WPARAM key) {}
    virtual void OnKeyUp(WPARAM key) {}
    virtual void OnMouseMove(int xPos, int yPos, WPARAM buttonState) {}
    virtual void OnMouseBtnDown(int x, int y, WPARAM button) {}
    virtual void OnMouseBtnUp(int x, int y, WPARAM button) {}

    // Accessors.
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    IUIRenderer* GetUIRenderer() const { return m_uiRenderer.get(); }
    ID3D12Resource* CurrentBackbuffer() const { return m_renderTargets[m_frameIndex].Get(); }

protected:
    virtual void CreateQueues() = 0;
    virtual ID3D12CommandQueue* GetPresentQueue() const = 0;

    // 파생 클래스가 선택적으로 구현
    virtual void OnAfterSwapchainCreated() {}     // 오프스크린/힙/업로드/프레임 자원
    virtual void OnInitPipelines() {}             // 공통 루트시그니처/PSO
    virtual void OnBuildInitialScene() {}         // 초기 씬 구성

	std::wstring GetAssetFullPath(LPCWSTR assetName);
	void GetHawrdwardAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHightPerformanceAdapter = false);
	void SetCustomWindowText(LPCWSTR text) const;
    void CreateDevice();
    void CreateSwapChain(HWND hwnd, ID3D12CommandQueue* presentQueue);
    void CreateBackbuffersAndDefaultDSV(UINT width, UINT height);
    void DestroyBackbuffersAndDefaultDSV();
    void CreateFenceAndEvent();
    void DestroyFenceAndEvent();
    void UpdateFrameIndexFromSwapchain();
    Timer& GetTimer() { return m_timer; }
    const Timer& GetTimer() const { return m_timer; }

protected:
    static const UINT kFrameCount = 2;
	UINT m_width;
	UINT m_height;
	float m_aspectRatio;
	bool m_userWarpDevice = false;

    // DirectX12 Common
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    UINT m_rtvDescriptorSize = 0;
    UINT m_dsvDescriptorSize = 0;

    ComPtr<ID3D12Resource> m_renderTargets[kFrameCount];
    ComPtr<ID3D12Resource> m_depthStencil;

    DXGI_FORMAT m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    ComPtr<ID3D12Fence> m_swapChainFence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValues[kFrameCount];
    UINT64 m_nextFenceValue = 0;
    UINT m_frameIndex = 0u;

    BOOL m_tearingSupported = FALSE;

    // UI
    std::unique_ptr<IUIRenderer> m_uiRenderer;

private:
	std::wstring m_title;
    Timer m_timer;
};

