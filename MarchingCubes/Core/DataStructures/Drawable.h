#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "Core/Rendering/Material.h"

struct DrawBindingInfo
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW  ibv;
	D3D12_PRIMITIVE_TOPOLOGY topology;
	UINT indexCount;

	D3D12_GPU_VIRTUAL_ADDRESS objectCBGpuVA;

	Material* material; // 혹은 Material& 등. 현재 Mesh는 shared_ptr<Material>을 들고 있음. :contentReference[oaicite:5]{index=5}
};

class GeometryBuffer
{
public:
	GeometryBuffer();
	GeometryBuffer(ID3D12Device* device, const GeometryData& data);
	~GeometryBuffer();

	void ReleaseGPUResources();
	void BindVBSlice(const ResourceSlice& inNew, ResourceSlice* outOld = nullptr);
	void BindIBSlice(const ResourceSlice& inNew, ResourceSlice* outOld = nullptr);
	void BindCBSlice(const ResourceSlice& inNew, ResourceSlice* outOld = nullptr);
	void ClearSlices();

	D3D12_GPU_VIRTUAL_ADDRESS GetObjectCBGpuVA() const { return m_objectCBGpuVA; }

	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; };
	void SetTopology(D3D12_PRIMITIVE_TOPOLOGY topology) { m_topology = topology; };

	ResourceSlice GetCurrentVBSlice() const { return m_vb; }
	ResourceSlice GetCurrentIBSlice() const { return m_ib; }
	ResourceSlice GetCurrentCBSlice() const { return m_cb; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return m_vertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIBV() const { return m_indexBufferView; }

private:
	// VB/IB
	ResourceSlice m_vb{}, m_ib{};
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW  m_indexBufferView{};
	D3D12_PRIMITIVE_TOPOLOGY m_topology;

	// Object Constants Buffer
	ResourceSlice m_cb{};
	D3D12_GPU_VIRTUAL_ADDRESS m_objectCBGpuVA = 0ull;
};

struct IDrawable
{
public:
	virtual ~IDrawable() = default;
	virtual DrawBindingInfo GetDrawBinding() const = 0;
	virtual ObjectConstants  GetObjectConstants() const = 0;
	virtual const GeometryData* GetCPUData() const { return nullptr; }
	virtual void SetCPUData(GeometryData&& data) {};
	virtual void SetCPUData(const GeometryData& data) {};
	virtual GeometryBuffer* GetGPUBuffer() { return nullptr; }

	virtual void SetUploadPending(bool pending) {}
	virtual bool IsUploadPending() const { return false; }
	virtual const char* GetDebugName() const = 0;
	virtual void SetDebugName(const std::string& name) {}
};

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const IDrawable* drawable);