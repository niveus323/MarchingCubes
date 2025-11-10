#include "pch.h"
#include "Drawable.h"
#include "Core/DataStructures/Data.h"
#include "Core/Rendering/UploadContext.h"

void RecordDrawItem(ID3D12GraphicsCommandList* cmdList, const IDrawable* drawable)
{
	if (!drawable || !cmdList) return;

	const DrawBindingInfo bind = drawable->GetDrawBinding();
	if (bind.objectCBGpuVA != 0ull)
	{
		cmdList->SetGraphicsRootConstantBufferView(1, bind.objectCBGpuVA);
	}

	if (bind.material)
	{
		bind.material->BindConstant(cmdList); //SetGraphicsRootConstantBufferView(2, material buffer GPUVA)
	}

	if (bind.indexCount > 0)
	{
		cmdList->IASetVertexBuffers(0, 1, &bind.vbv);
		cmdList->IASetIndexBuffer(&bind.ibv);
		cmdList->IASetPrimitiveTopology(bind.topology);
		cmdList->DrawIndexedInstanced(bind.indexCount, 1, 0, 0, 0);
	}
}

GeometryBuffer::GeometryBuffer() :
	m_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
{
}

GeometryBuffer::GeometryBuffer(ID3D12Device* device, const GeometryData& data) :
	m_topology(data.topology)
{
}

GeometryBuffer::~GeometryBuffer()
{
}

void GeometryBuffer::ReleaseGPUResources()
{
	ClearSlices();
}

void GeometryBuffer::BindVBSlice(const ResourceSlice& inNew, ResourceSlice* outOld)
{
	if (outOld) *outOld = m_vb;
	m_vb = inNew;
	m_vertexBufferView.BufferLocation = m_vb.res ? m_vb.res->GetGPUVirtualAddress() + m_vb.offset : 0;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = m_vb.size;
}

void GeometryBuffer::BindIBSlice(const ResourceSlice& inNew, ResourceSlice* outOld)
{
	if (outOld) *outOld = m_ib;
	m_ib = inNew;
	m_indexBufferView.BufferLocation = m_ib.res ? (m_ib.res->GetGPUVirtualAddress() + m_ib.offset) : 0;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = m_ib.size;
}

void GeometryBuffer::BindCBSlice(const ResourceSlice& inNew, ResourceSlice* outOld)
{
	if (outOld) *outOld = m_cb;
	m_cb = inNew;
	m_objectCBGpuVA = m_cb.gpuVA;
}

void GeometryBuffer::ClearSlices()
{
	m_vb = {};
	m_ib = {};
	m_vertexBufferView = {};
	m_indexBufferView = {};
}
