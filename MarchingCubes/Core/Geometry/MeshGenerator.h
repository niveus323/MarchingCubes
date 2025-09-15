#pragma once
#include "Core/DataStructures/Data.h"
#include <DirectXMath.h>
#include <WTypesbase.h>

namespace MeshGenerator
{
	MeshData CreateSphereMeshData(float radius, const DirectX::XMFLOAT4& color = { 1,1,1,1 }, UINT sliceCount = 12, UINT stackCount = 6);
	MeshData GenerateCubeGrid(int rows, int cols, int layers);
	
}
