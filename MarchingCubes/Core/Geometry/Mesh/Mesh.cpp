#include "pch.h"
#include "Mesh.h"
#include "Core/Math/PhysicsHelper.h"

Mesh::Mesh(ID3D12Device* device, const GeometryData& data) :
	m_cpu(data),
	m_buffer(device, data)
{
	// Default CB
	XMMATRIX identity = XMMatrixIdentity();
	XMStoreFloat4x4(&m_objectCB.worldMatrix, identity);
	XMStoreFloat4x4(&m_objectCB.worldInvMatrix, XMMatrixInverse(nullptr, identity));
}

Mesh::Mesh(ID3D12Device* device, const GeometryData& data, const ObjectConstants& cb) :
	m_cpu(data),
	m_buffer(device, data),
	m_objectCB(cb)
{
}

DrawBindingInfo Mesh::GetDrawBinding() const
{
	DrawBindingInfo info{};
	const BufferHandle& vb = m_buffer.GetCurrentVBHandle();
	info.vbv = {
		.BufferLocation = vb.res ? vb.res->GetGPUVirtualAddress() + vb.offset : 0,
		.SizeInBytes = vb.size,
		.StrideInBytes = static_cast<UINT>(sizeof(Vertex)),
	};

	const BufferHandle& ib = m_buffer.GetCurrentIBHandle();
	info.ibv = {
		.BufferLocation = ib.res ? (ib.res->GetGPUVirtualAddress() + ib.offset) : 0,
		.SizeInBytes = static_cast<UINT>(ib.size),
		.Format = DXGI_FORMAT_R32_UINT
	};
	info.topology = m_buffer.GetTopology();
	info.indexCount = static_cast<UINT>(m_cpu.indices.size());
	info.objectCBGpuVA = m_buffer.GetCurrentCBHandle().gpuVA;
	return info;
}

void Mesh::UpdateConstants()
{
	XMMATRIX worldTrans = XMMatrixTranspose(GetWorldMatrix());
	XMMATRIX worldInvTrans = XMMatrixTranspose(GetWorldInvMatrix());

	XMStoreFloat4x4(&m_objectCB.worldMatrix, worldTrans);
	XMStoreFloat4x4(&m_objectCB.worldInvMatrix, worldInvTrans);
}

DirectX::XMMATRIX Mesh::GetWorldMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX R = XMMatrixRotationQuaternion(ToQuatFromEuler(m_rotation));
	XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	return S * R * T;
}

DirectX::XMMATRIX Mesh::GetWorldInvMatrix() const
{
	return XMMatrixInverse(nullptr, GetWorldMatrix());
}

void Mesh::SetColor(const DirectX::XMFLOAT4& color)
{
	for (auto& v : m_cpu.vertices)
	{
		v.color = color;
	}
}

void Mesh::Move(const DirectX::XMFLOAT3& delta)
{
	m_position.x += delta.x;
	m_position.y += delta.y;
	m_position.z += delta.z;
}

void Mesh::Rotate(const DirectX::XMVECTOR& deltaQuat)
{
	using namespace DirectX;
	XMVECTOR current = ToQuatFromEuler(m_rotation);
	XMVECTOR result = XMQuaternionNormalize(XMQuaternionMultiply(current, deltaQuat));
	m_rotation = ToEulerFromQuat(result);
}

void Mesh::Scale(const DirectX::XMFLOAT4& scaleFactor)
{
	m_scale.x *= scaleFactor.x;
	m_scale.y *= scaleFactor.y;
	m_scale.z *= scaleFactor.z;
}

void Mesh::BuildTriBounds()
{
	// AABB BroadPhase를 위한 Bound 세팅
	const auto& vertices = m_cpu.vertices;
	const auto& indices = m_cpu.indices;
	size_t triCount = indices.size() / 3;
	m_triBounds.clear();
	m_triBounds.resize(triCount);

	for (size_t t = 0; t < triCount; ++t)
	{
		XMFLOAT3 points[3]
		{
			vertices[indices[3 * t + 0]].pos,
			vertices[indices[3 * t + 1]].pos,
			vertices[indices[3 * t + 2]].pos
		};

		BoundingBox::CreateFromPoints(m_triBounds[t], 3, points, sizeof(XMFLOAT3));
	}
}