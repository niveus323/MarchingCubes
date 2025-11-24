#pragma once
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/Memory/CommonMemory.h"
#include "ShaderTypes.h"
#include <d3d12.h>
#include <d3dcommon.h>
#include <Windows.h>
#include <string>
#include <utility>

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
	GeometryBuffer() : m_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}
	GeometryBuffer(ID3D12Device* device, const GeometryData& data) : m_topology(data.topology) {}
	~GeometryBuffer() = default;

	void ReleaseGPUResources() { ClearHandles(); }
	void SwapVBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_vb); }
	void SwapIBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_ib); }
	void SwapCBHandle(BufferHandle& newHandle) { std::swap(newHandle, m_cb); }
	void ClearHandles()
	{
		m_vb = {}; m_ib = {}; m_cb = {};
	}

	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_topology; };
	void SetTopology(D3D12_PRIMITIVE_TOPOLOGY topology) { m_topology = topology; };

	BufferHandle GetCurrentVBHandle() const { return m_vb; }
	BufferHandle GetCurrentIBHandle() const { return m_ib; }
	BufferHandle GetCurrentCBHandle() const { return m_cb; }

private:
	BufferHandle m_vb{}, m_ib{}, m_cb{};
	D3D12_PRIMITIVE_TOPOLOGY m_topology;
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

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const DrawBindingInfo& bi);