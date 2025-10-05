#pragma once
#include "App/Common/DXAppBase.h"
#include "App/Editor/Interfaces/EditorInterfaces.h"
#include "Core/Rendering/Camera.h"
#include "Core/Rendering/LightManager.h"
#include "Core/Geometry/Mesh.h"
#include "Core/Input/InputState.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/Geometry/MarchingCubes/TerrainSystem.h"
#include <unordered_map>
#include <array>

#include "Core/Geometry/UploadRing.h"
using Microsoft::WRL::ComPtr;

class MCTerraformEditor : public DXAppBase
{
public:
    MCTerraformEditor(UINT width, UINT height, std::wstring name);
    ~MCTerraformEditor();

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
    
    //Marching Cubes
    std::shared_ptr<_GRD> MakeSphereGrid(unsigned int N, float cell, float radius, XMFLOAT3 origin, GridDesc& OutGridDesc);

private:
    static const UINT FrameCount = 2;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_graphicsQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencil;
    std::unordered_map<PipelineMode, ComPtr<ID3D12PipelineState>> m_pipelineStates;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    std::unique_ptr<BundleRecorder> m_bundleRecorder;
    std::unordered_map < PipelineMode, std::vector<StaticRenderItem>> m_staticRenderItems;
    std::unordered_map < PipelineMode, std::vector<DynamicRenderItem>> m_dynamicRenderItems;
    std::vector<PendingDeleteItem> m_pendingDeletes;
    std::vector<ComPtr<ID3D12Resource>> m_toDeletesContainer;
    UINT m_rtvDescriptorSize;

    UploadContext m_uploadContext;

    // StaticSampler용 Heap
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_swapChainFence;
    UINT64 m_fenceValues[FrameCount];

    // Scene Objects
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<LightManager> m_lightManager;

    std::vector<std::unique_ptr<IDrawable>> m_StaticObjects;

    // TODO : PBR 테스트를 위해 임시로 Material을 app에서 초기화하여 사용, ResoureManager 및 Wrapper 클래스로 만들어서 .mat 파일 로드하여 인스턴스를 wrapper 클래스에 넘기는 방식으로 수정할 것.
    std::shared_ptr<Material> m_defaultMat;

    // Keyboard & Mouse Input Object
    InputState m_inputState;

    // Marching Cubes
    DirectX::XMFLOAT3 m_gridOrigin = { 0,0,0 };
    std::array<int, 3> m_gridSize = { 1,1,1 };
    int m_cellSize = 1;
    float m_brushRadius = 3.0f;
    float m_brushStrength = 5.0f;
    std::array<float, 3> m_lightDir = { -1.0f, -1.0f, -1.0f };
    float m_mcIso = 0.0f;

    // UI
    bool m_gridRenewRequested = false;

    // debug
    bool m_debugViewEnabled = false;
    bool m_debugNormalEnabled = false;
    ComPtr<ID3D12PipelineState> m_wireFramePSO;
    ComPtr<ID3D12PipelineState> m_debugNormalPSO;

    std::unique_ptr<TerrainSystem> m_terrain;

    UploadRing m_uploadRing;
    std::vector<std::pair<UINT64, UINT64>> m_allocationsThisSubmit;

    ComPtr<ID3D12Fence> m_uploadFence;

#ifdef _DEBUG
    std::unique_ptr<Mesh> m_debugBrush;
#endif // _DEBUG

};