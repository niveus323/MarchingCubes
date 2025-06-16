#pragma once
#include "Core/DataStructures/Data.h"
#include <DirectXMath.h>
#include <WTypesbase.h>

MeshData CreateSphereMeshData(float radius, UINT sliceCount = 12, UINT stackCount = 6)
{
	MeshData mesh;

	mesh.vertices.push_back({ {0.0f, radius, 0.0f}, {1,1,1,1} });

	for (UINT stack = 1; stack < stackCount; ++stack)
	{
		float phi = DirectX::XM_PI * stack / stackCount;
		for (UINT slice = 0; slice <= sliceCount; ++slice)
		{
			float theta = DirectX::XM_2PI * slice / sliceCount;
			float x = radius * sinf(phi) * cosf(theta);
			float y = radius * cosf(phi);
			float z = radius * sinf(phi) * sinf(theta);

			mesh.vertices.push_back({ {x,y,z}, {1,1,1,1} });
		}
	}

	mesh.vertices.push_back({ {0.0f, -radius, 0.0f}, {1,1,1,1} });

	for (UINT i = 1; i <= sliceCount; ++i)
	{
		mesh.indices.push_back(0);
		mesh.indices.push_back(i);
		mesh.indices.push_back(i + 1);
	}

	UINT ringVertexCount = sliceCount + 1;
	for (UINT stack = 0; stack < stackCount - 2; ++stack)
	{
		for (UINT slice = 0; slice < sliceCount; ++slice)
		{
			UINT i0 = 1 + stack * ringVertexCount + slice;
			UINT i1 = i0 + 1;
			UINT i2 = i0 + ringVertexCount;
			UINT i3 = i2 + 1;

			mesh.indices.push_back(i0);
			mesh.indices.push_back(i2);
			mesh.indices.push_back(i1);

			mesh.indices.push_back(i1);
			mesh.indices.push_back(i2);
			mesh.indices.push_back(i3);
		}
	}

	UINT southPoleIndex = (UINT)mesh.vertices.size() - 1;
	UINT baseIndex = 1 + (stackCount - 2) * ringVertexCount;
	for (UINT i = 0; i < sliceCount; ++i)
	{
		mesh.indices.push_back(southPoleIndex);
		mesh.indices.push_back(baseIndex + i + 1);
		mesh.indices.push_back(baseIndex + i);
	}

	return mesh;
}