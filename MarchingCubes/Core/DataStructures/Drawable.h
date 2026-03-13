#pragma once
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/DataStructures/Data.h"

struct DrawBindingInfo
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW  ibv;
	D3D12_PRIMITIVE_TOPOLOGY topology;
	UINT indexCount;
};

class GeometryBuffer
{
public:
	GeometryBuffer() = default;
	~GeometryBuffer() {
		ReleaseGPUResources();
	};

	void ReleaseGPUResources() { m_vb = {}; m_ib = {}; }
	void SetLayout(uint32_t vStride, DXGI_FORMAT iFormat)
	{
		m_vertexStride = vStride;
		m_indexFormat = iFormat;
	}
	void SwapVBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_vb); }
	void SwapIBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_ib); }
	
	BufferHandle GetVBHandle() const { return m_vb; }
	BufferHandle GetIBHandle() const { return m_ib; }

	D3D12_VERTEX_BUFFER_VIEW GetVBV() const
	{
		if (!m_vb.res) return {};
		return D3D12_VERTEX_BUFFER_VIEW{
			.BufferLocation = m_vb.res->GetGPUVirtualAddress() + m_vb.offset,
			.SizeInBytes = static_cast<UINT>(m_vb.size),
			.StrideInBytes = m_vertexStride // 저장된 Stride 사용
		};
	}

	D3D12_INDEX_BUFFER_VIEW GetIBV() const
	{
		if (!m_ib.res) return {};
		return D3D12_INDEX_BUFFER_VIEW{
			.BufferLocation = m_ib.res->GetGPUVirtualAddress() + m_ib.offset,
			.SizeInBytes = static_cast<UINT>(m_ib.size),
			.Format = m_indexFormat // 저장된 Format 사용
		};
	}

private:
	BufferHandle m_vb{}, m_ib{};
	uint32_t m_vertexStride = sizeof(Vertex);
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;
};

struct RenderItem
{
	GeometryBuffer* meshBuffer = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	uint32_t indexCount = 0;
	uint32_t indexOffset = 0;
	uint32_t baseVertexLocation = 0;
	uint32_t instanceCount = 1;
	uint32_t materialIndex = 0;
	uint32_t objectID = 0;
	std::string debugName;

	std::vector<ShaderBinding> resourceBindings; // 리소스 바인딩 정보

	void Reset()
	{
		meshBuffer = nullptr;
		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		indexCount = 0;
		indexOffset = 0;
		baseVertexLocation = 0;
		instanceCount = 1;
		materialIndex = 0;
		objectID = 0;
		debugName.clear();
		resourceBindings.clear();
	}
};

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const DrawBindingInfo& bi);
void DrawItem(ID3D12GraphicsCommandList* cmd, const RenderItem* item);