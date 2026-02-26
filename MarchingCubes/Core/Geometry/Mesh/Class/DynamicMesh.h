#pragma once
#include "Core/Geometry/Mesh/Class/Mesh.h"
class DynamicMesh : public Mesh
{
	friend class MeshRegistry;
public:
	using Mesh::Mesh;
private:
	void SwapBuffer(const GeometryBuffer& buffer, const GeometryData& data, const std::vector<MeshSubmesh>& submeshes);
};

