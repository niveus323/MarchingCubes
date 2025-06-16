#pragma once
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
	//DirectX::XMFLOAT3 normal;
};

struct MeshData
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};