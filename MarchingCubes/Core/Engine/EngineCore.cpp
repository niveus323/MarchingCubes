#include "pch.h"
#include "EngineCore.h"
uint32_t EngineCore::s_frameIndex = 0; 
float EngineCore::s_deltaTime = 0.0f;
ID3D12Device* EngineCore::s_device = nullptr;
ID3D12Fence* EngineCore::s_swapChainFence = nullptr;
uint64_t* EngineCore::s_nextFenceValue = nullptr;
RenderSystem* EngineCore::s_renderSystem = nullptr;
ResourceManager* EngineCore::s_resourceManager = nullptr;
InputState* EngineCore::s_inputState = nullptr; 
UploadContext* EngineCore::s_uploadContext = nullptr;
GpuAllocator* EngineCore::s_gpuAllocator = nullptr;
DescriptorAllocator* EngineCore::s_descriptorAllocator = nullptr;
std::unordered_map<std::type_index, ISubSystem*> EngineCore::s_subsystems;