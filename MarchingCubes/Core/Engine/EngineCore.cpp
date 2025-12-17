#include "pch.h"
#include "EngineCore.h"
ID3D12Device* EngineCore::s_device = nullptr;
RenderSystem* EngineCore::s_renderSystem = nullptr;
ResourceManager* EngineCore::s_resourceManager = nullptr;
InputState* EngineCore::s_inputState = nullptr; 
uint32_t EngineCore::s_frameIndex = 0; 
UploadContext* EngineCore::s_uploadContext = nullptr;
DescriptorAllocator* EngineCore::s_descriptorAllocator = nullptr;