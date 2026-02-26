#include "pch.h"
#include "Mesh.h"
#include "Core/Rendering/UploadContext.h"
#include "Core/DataStructures/Data.h"
#include "Core/Engine/EngineCore.h"
#include "Core/Rendering/UploadContext.h"

Mesh::Mesh(const std::string& name) : 
	m_debugName(name)
{
}

void Mesh::Initialize(const GeometryBuffer& buffer, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes)
{
	m_buffer = buffer;
	m_submeshes = submeshes;
	m_topology = data.topology;
	BuildTriBounds(data); // TODO : 이것도 GeometryData 측에 두는게 좋아보임.
#ifdef _DEBUG
	m_cpuData = data;
#endif // _DEBUG

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