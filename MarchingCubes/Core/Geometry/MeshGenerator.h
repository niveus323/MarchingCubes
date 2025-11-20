#pragma once
#include "Core/DataStructures/Data.h"
#include <DirectXMath.h>
#include <WTypesbase.h>

namespace MeshGenerator
{
	GeometryData CreateSphereMeshData(float radius, const DirectX::XMFLOAT4& color = { 1,1,1,1 }, uint32_t sliceCount = 12, uint32_t stackCount = 6);
	GeometryData GenerateCubeGrid(int rows, int cols, int layers);
	
}
