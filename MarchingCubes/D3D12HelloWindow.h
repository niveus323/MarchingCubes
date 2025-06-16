#pragma once
#include "DXAppBase.h"
#include "Core/Rendering/Camera.h"
#include "Core/Geometry/Mesh.h"
#include "Core/Input/InputState.h"
#include "App/Editor/Handles/VertexSelector.h"
#include <unordered_map>
using Microsoft::WRL::ComPtr;

class D3D12HelloWindow : public DXAppBase
{
public:
	D3D12HelloWindow(UINT width, UINT height, std::wstring name);

	virtual void OnInit() override;
    virtual void OnInitUI() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRender() override;
    virtual void OnRenderUI() override;
	virtual void OnDestroy() override;
    virtual void OnKeyDown(WPARAM key) override;
    virtual void OnKeyUp(WPARAM key) override;
    virtual void OnMouseMove(int xPos, int yPos, WPARAM buttonState) override;
    virtual void OnMouseBtnDown(int x, int y, WPARAM button) override;
    virtual void OnMouseBtnUp(int x, int y, WPARAM button) override;
private:
    void LoadPipeline();
    void LoadAssets();
    void CreatePipelineStates();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
    
    // MousePicking
    void CreateIDRenderTarget();
    void RenderForPicking();
    uint32_t ReadPickedID(int mouseX, int mouseY);
    void HandlePickedObject(uint32_t pickedID);
    XMFLOAT4 EncodeIDColor(uint32_t id);
    uint32_t DecodeIDColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

private:
    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::unordered_map<PipelineMode, ComPtr<ID3D12PipelineState>> m_pipelineStates;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    std::unordered_map < PipelineMode, std::vector<ComPtr<ID3D12GraphicsCommandList>>> m_bundles;
    UINT m_rtvDescriptorSize;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    // Scene Objects
    Camera* m_camera;
    Mesh m_cubeMesh;

    // Keyboard & Mouse Input Object
    InputState m_inputState;

    // Mouse Picking
    ComPtr<ID3D12Resource> m_idRenderTarget;
    ComPtr<ID3D12RootSignature> m_idRootsignature;
    ComPtr<ID3D12DescriptorHeap> m_idRTVHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_idRTVHandle;
    ComPtr<ID3D12PipelineState> m_idPipelineState;

    std::vector<std::unique_ptr<VertexSelector>> m_pickables;
    VertexSelector* m_selectedObject;

    //debug
    bool m_debugViewEnabled = false;
};

