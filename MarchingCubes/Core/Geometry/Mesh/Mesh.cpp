#include "pch.h"
#include "Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/DataStructures/Data.h"

Mesh::Mesh(UploadContext* uploadcontext, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes, const std::string& name) :
	m_topology(data.topology),
	m_submeshes(submeshes),
	m_debugName(name)
{
	if (m_submeshes.empty())
	{
		MeshSubmesh sm;
		sm.indexCount = static_cast<uint32_t>(data.indices.size());
		sm.indexOffset = 0;
		sm.baseVertexLocation = 0;
		sm.materialIndex = 0;
		m_submeshes.push_back(sm);
	}

	if (uploadcontext)
	{
		if (m_debugName.empty())
		{
			//임시 이름 세팅
			SetDebugName("MeshInstance");
		}
		uploadcontext->UploadGeometry(&m_buffer, data, m_debugName);
	}

	BuildTriBounds(data);
}


Mesh::Mesh(UploadContext* uploadcontext, const GeometryData& data, const std::string& name) : 
	Mesh(uploadcontext, data, {}, name)
{
}

void Mesh::UpdateData(UploadContext* uploadcontext, const GeometryData& data)
{
	m_topology = data.topology;
	if (m_submeshes.size() == 1)
	{
		m_submeshes[0].indexCount = static_cast<uint32_t>(data.indices.size());
	}

	if (uploadcontext)
	{
		uploadcontext->UploadGeometry(&m_buffer, data, m_debugName);
	}
	BuildTriBounds(data);
}

void Mesh::BuildTriBounds(const GeometryData& data)
{
	// AABB BroadPhase를 위한 Bound 세팅
	m_triBounds.clear();
	m_triBounds.reserve(m_submeshes.size());

	const auto& vertices = data.vertices;
	const auto& indices = data.indices;
	for (const auto& submesh : m_submeshes)
	{
		// 유효하지 않은 서브메쉬 처리
		if (submesh.indexCount == 0)
		{
			m_triBounds.emplace_back(BoundingBox());
			continue;
		}
		XMVECTOR vMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
		XMVECTOR vMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

		for (uint32_t i = 0; i < submesh.indexCount; ++i)
		{
			uint32_t vertexIndex = submesh.baseVertexLocation + indices[submesh.indexOffset + i];
			if (vertexIndex < vertices.size())
			{
				XMVECTOR vPos = XMLoadFloat3(&vertices[vertexIndex].pos);
				vMin = XMVectorMin(vMin, vPos);
				vMax = XMVectorMax(vMax, vPos);
			}
		}

		// 계산된 Min, Max를 기반으로 AABB 생성
		BoundingBox submeshBox;
		BoundingBox::CreateFromPoints(submeshBox, vMin, vMax);

		m_triBounds.push_back(submeshBox);
	}
}