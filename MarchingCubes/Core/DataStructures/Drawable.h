#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "ShaderTypes.h"

struct DrawBindingInfo
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW  ibv;
	D3D12_PRIMITIVE_TOPOLOGY topology;
	UINT indexCount;
	D3D12_GPU_VIRTUAL_ADDRESS objectCBGpuVA;
};

class GeometryBuffer
{
public:
	GeometryBuffer() = default;
	~GeometryBuffer() {
		ReleaseGPUResources();
	};

	void ReleaseGPUResources() { m_vb = {}; m_ib = {}; m_cb = {}; }
	void SwapVBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_vb); }
	void SwapIBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_ib); }
	void SwapCBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_cb); }

	BufferHandle GetVBHandle() const { return m_vb; }
	BufferHandle GetIBHandle() const { return m_ib; }
	BufferHandle GetCBHandle() const { return m_cb; }

private:
	BufferHandle m_vb{}, m_ib{}, m_cb{};
};

struct RenderItem
{
	GeometryBuffer* meshBuffer = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	uint32_t indexCount = 0;
	uint32_t indexOffset = 0;
	uint32_t baseVertexLocation = 0;
	uint32_t instanceCount = 1;
	DirectX::XMFLOAT4X4 worldMatrix = {
		1,0,0,0, 
		0,1,0,0, 
		0,0,1,0, 
		0,0,0,1
	};
	uint32_t materialIndex = 0;
	std::string debugName;
};

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const DrawBindingInfo& bi);
void DrawItem(ID3D12GraphicsCommandList* cmd, const RenderItem& item);