#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT4 color;
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
	size_t size;
};
