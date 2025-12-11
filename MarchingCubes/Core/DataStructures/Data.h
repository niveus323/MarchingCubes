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
	DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};
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
	uint32_t materialIndex = 0;
};

struct Transform
{
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

static uint32_t kMaxLights = 256u;
struct LightBlobView
{
	const void* data;
	uint32_t size;
};

struct MaterialInstance
{
	uint32_t index = 0;
	std::string psoName = "Filled";
};