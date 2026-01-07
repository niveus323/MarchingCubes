#pragma once
#include "Core/Engine/Subsystem/SubSystem.h"
#include <unordered_map>
#include <typeindex>

//Forward Declaration
class RenderSystem;
class ResourceManager;
class UploadContext;
class DescriptorAllocator;
class InputState;
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

    template<std::derived_from<ISubSystem> T>
    static void RegisterSubsystem(T* system)
    {
        system->Initialize();
        s_subsystems[typeid(T)] = system;
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
    // 실제 정적 포인터 변수들
    static ID3D12Device* s_device;
    static RenderSystem* s_renderSystem;
    static ResourceManager* s_resourceManager;
    static InputState* s_inputState; // Scene 혹은 GameMode에서 Input을 받도록 하는게?
    static UploadContext* s_uploadContext;
    static DescriptorAllocator* s_descriptorAllocator; // Scene의 PrepareRender에서만 필요
    static uint32_t s_frameIndex;

    static std::unordered_map<std::type_index, ISubSystem*> s_subsystems;
};

