#include "pch.h"
#include "DynamicMesh.h"

void DynamicMesh::SwapBuffer(const GeometryBuffer& buffer, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes)
{
    m_buffer = buffer;
    m_topology = data.topology;
    m_submeshes = submeshes;
    BuildTriBounds(data);
}
