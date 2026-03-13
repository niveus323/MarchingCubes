#pragma once
#include <vector>
#include <d3dcommon.h>
#include <DirectXMath.h>
#include <cstdint>

struct Vertex
{
	DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 normal{0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT4 tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT2 texCoord{0.0f, 0.0f};
};

struct GeometryData
{
	std::vector<Vertex> vertices = {};
	std::vector<uint32_t> indices = {};
	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

struct MeshSubmesh
{
	uint32_t indexCount = 0;
	uint32_t indexOffset = 0;
	uint32_t baseVertexLocation = 0;
	uint32_t materialslot = 0;
};

static uint32_t kMaxLights = 256u;
struct LightBlobView
{
	const void* data;
	uint32_t size;
};

enum class EBindingType
{
	CONSTANTS, // Root Constants
	CBV,	   // Root Constant Buffer View
	SRV,	   // Root Shader Resource View (Texture/Buffer)
	UAV,	   // Root Unordered Access View
	TABLE
};

struct ShaderBinding
{
	EBindingType type;
	uint32_t rootParameterIndex;
	union {
		uint32_t constantData;							 // Root Constants 사용 시 해당 필드
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;			 // Root Descriptor 사용 시 해당 필드
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle; // Decriptor Table 사용 시 해당 필드
	};
};