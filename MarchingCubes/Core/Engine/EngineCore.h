#pragma once

//Forward Declaration
class RenderSystem;
class ResourceManager;
class UploadContext;
class DescriptorAllocator;
class InputState;
class IUIRenderer;
struct ID3D12Device;

class EngineCore
{
public:
    static ID3D12Device* GetDevice() { return s_device; }
    static RenderSystem* GetRenderSystem() { return s_renderSystem; }
    static ResourceManager* GetResourceManager() { return s_resourceManager; }
    static InputState* GetInputState() { return s_inputState; }
    static UploadContext* GetUploadContext() { return s_uploadContext; }
    static DescriptorAllocator* GetDescriptorAllocator() { return s_descriptorAllocator; }
    static uint32_t GetFrameIndex() { return s_frameIndex; }

    static void SetDevice(ID3D12Device* device) { s_device = device; }
    static void SetRenderSystem(RenderSystem* rs) { s_renderSystem = rs; }
    static void SetResourceManager(ResourceManager* rm) { s_resourceManager = rm; }
    static void SetInputState(InputState* input) { s_inputState = input; }
    static void SetUploadContext(UploadContext* uc) { s_uploadContext = uc; }
    static void SetDescriptorAllocator(DescriptorAllocator* da) { s_descriptorAllocator = da; }
    static void SetFrameIndex(uint32_t frameIndex) { s_frameIndex = frameIndex; }

private:
    // 실제 정적 포인터 변수들
    static ID3D12Device* s_device;
    static RenderSystem* s_renderSystem;
    static ResourceManager* s_resourceManager;
    static InputState* s_inputState; // Scene 혹은 GameMode에서 Input을 받도록 하는게?
    static UploadContext* s_uploadContext;
    static DescriptorAllocator* s_descriptorAllocator; // Scene의 PrepareRender에서만 필요
    static uint32_t s_frameIndex;
};

