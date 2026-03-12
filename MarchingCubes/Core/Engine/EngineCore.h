#pragma once
#include "Core/Engine/Subsystem/SubSystem.h"
#include <unordered_map>
#include <typeindex>

//Forward Declaration
class RenderSystem;
class ResourceManager;
class UploadContext;
class GpuAllocator;
class DescriptorAllocator;
class InputState;
struct ID3D12Device;

class EngineCore
{
public:
    static uint32_t GetFrameIndex() { return s_frameIndex; }
    static void SetFrameIndex(uint32_t frameIndex) { s_frameIndex = frameIndex; }

    static float GetDeltaTime() { return s_deltaTime; }
    static void SetDeltaTime(float deltaTime) { s_deltaTime = deltaTime; }

    static ID3D12Device* GetDevice() { return s_device; }
    static void SetDevice(ID3D12Device* device) { s_device = device; }

    static ID3D12Fence* GetSwapChainFence() { return s_swapChainFence; }
    static void SetSwapChainFence(ID3D12Fence* fence) { s_swapChainFence = fence; }

    static uint64_t GetNextFenceValue() { return *s_nextFenceValue + 1; }
    static void SetNextFenceValuePtr(uint64_t* fenceValuePtr) { s_nextFenceValue = fenceValuePtr; }

    static RenderSystem* GetRenderSystem() { return s_renderSystem; }
    static void SetRenderSystem(RenderSystem* rs) { s_renderSystem = rs; }

    static ResourceManager* GetResourceManager() { return s_resourceManager; }
    static void SetResourceManager(ResourceManager* rm) { s_resourceManager = rm; }

    static InputState* GetInputState() { return s_inputState; }
    static void SetInputState(InputState* input) { s_inputState = input; }

    static UploadContext* GetUploadContext() { return s_uploadContext; }
    static void SetUploadContext(UploadContext* uc) { s_uploadContext = uc; }

    static GpuAllocator* GetGpuAllocator() { return s_gpuAllocator; }
    static void SetGpuAllocator(GpuAllocator* ga) { s_gpuAllocator = ga; }

    static DescriptorAllocator* GetDescriptorAllocator() { return s_descriptorAllocator; }
    static void SetDescriptorAllocator(DescriptorAllocator* da) { s_descriptorAllocator = da; }

    template<std::derived_from<ISubSystem> T>
    static void RegisterSubsystem(T* system)
    {
        system->Initialize();
        s_subsystems[typeid(T)] = system;
    }
    
    static void RegisterSubsystem(std::type_index typeInfo, ISubSystem* system)
    {
        system->Initialize();
        s_subsystems[typeInfo] = system;
    }

    template <std::derived_from<ISubSystem> T>
    static void UnregisterSubsystem()
    {
        UnregisterSubsystem(typeid(T));
    }
    
    static void UnregisterSubsystem(std::type_index type)
    {
        auto it = s_subsystems.find(type);
        if (it != s_subsystems.end())
        {
            s_subsystems.erase(it);
        }
    }

    template <std::derived_from<ISubSystem> T>
    static T* GetSubsystem()
    {
        return GetSubsystem(typeid(T));
    }

    static ISubSystem* GetSubsystem(std::type_index type)
    {
        auto it = s_subsystems.find(type);
        if (it != s_subsystems.end())
        {
            return it->second;
        }
        return nullptr;
    }

    static void UpdateSubsystems(float dt)
    {
        for (auto& [key, sys] : s_subsystems) sys->Update(dt);
    }

    static void ComputeSubsystems()
    {
        for (auto& [type, system] : s_subsystems)
        {
            system->ExecuteCompute(GetFrameIndex());
        }
    }

    static void ShutdownSubsystems()
    {
        // NOTE : 서브 시스템 간 종속성이 존재한다면 std::vector로 관리하여 순서대로 해제될 수 있도록 해야함.
        s_subsystems.clear();
    }


private:
    // 엔진 내부 값들(ex 프레임 인덱스, DeltaTime) 
    static uint32_t s_frameIndex;
    static float s_deltaTime;

    // 실제 정적 포인터 변수들
    static ID3D12Device* s_device;
    static ID3D12Fence* s_swapChainFence;
    static uint64_t* s_nextFenceValue;

    static RenderSystem* s_renderSystem;
    static ResourceManager* s_resourceManager;
    static InputState* s_inputState; // Scene 혹은 GameMode에서 Input을 받도록 하는게?
    static UploadContext* s_uploadContext;
    static GpuAllocator* s_gpuAllocator;
    static DescriptorAllocator* s_descriptorAllocator; // Scene의 PrepareRender에서만 필요

    static std::unordered_map<std::type_index, ISubSystem*> s_subsystems;
};

