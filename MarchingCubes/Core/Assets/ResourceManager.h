#pragma once
#include "Material.h"
#include "Core/Rendering/PSO/DescriptorAllocator.h"
#include "Core/Rendering/UploadContext.h"
#include <DirectXTex.h>

/* [ResourceManager]
* - LifeTime : Engine Load -> Engine UnLoad
* - OwnerShip : Engine
* - Access : Engine::GetResourceManager()
* - Responsibility : 
* 	- GeometryData : .obj 파일을 로드하여 소유/참조 전달
* 	- AnimData : .anim 파일을 로드하여 소유/참조 전달
* 	- SoundData : .wav 등 사운드 파일을 로드하여 소유/참조 전달
* 	- Texture : .dds, .png 등 텍스쳐 파일을 로드하여 D3D12_SUBRESOURCE_DATA를 소유/참조 전달
* 	- SceneData : .scene 과 같이 에디터로 작업한 내역을 저장 + 로드하여 SceneData를 소유/참조 전달
* 	- DataAsset : .xml, .tex 등 raw 데이터를 관리/참조 전달
*/

// Forward Declaration
class TextureRegistry;
class MaterialRegistry;

class ResourceManager
{
public:
	ResourceManager(ID3D12Device* device, UploadContext* uploadcontext, DescriptorAllocator* descriptorAllocator);
	~ResourceManager();

	void syncGpu(ID3D12GraphicsCommandList* cmd);
	void BindDescriptorTable(ID3D12GraphicsCommandList* cmd);
	void BuildTables(ID3D12GraphicsCommandList* cmd);

	uint32_t LoadTexture(const std::wstring& path);
	size_t AddMaterial(const MaterialCPU& material);

private:
	ID3D12Device* m_device = nullptr;
	UploadContext* m_uploadContext = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;
	std::unique_ptr<TextureRegistry> m_textureRegistry;
	std::unique_ptr<MaterialRegistry> m_materialRegistry;
};

