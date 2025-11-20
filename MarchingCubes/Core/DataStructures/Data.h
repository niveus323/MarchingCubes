#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 normal{0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT2 texCoord{0.0f, 0.0f};
	DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct GeometryData
{
	std::vector<Vertex> vertices = {};
	std::vector<uint32_t> indices = {};
	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

static uint32_t kMaxLights = 256u;
struct LightBlobView
{
	const void* data;
	uint32_t size;
};
