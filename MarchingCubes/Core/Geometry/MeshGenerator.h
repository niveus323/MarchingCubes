#pragma once
#include "Core/DataStructures/Data.h"
#include <DirectXMath.h>
#include <WTypesbase.h>

namespace MeshGenerator
{
	enum class PrimitiveTypes : uint8_t
	{
		SolidSphere = 0,
		SolidCube,

		CubeGrid,
	};

	//-------- Solid --------

	GeometryData CreateSphereMeshData(float radius = 1.0f, uint32_t sliceCount = 12, uint32_t stackCount = 6);
	GeometryData CreateSolidCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

	//-------- Wireframe -------- 
	GeometryData CreateWireSphere(float radius = 1.0f, uint32_t sliceCount = 12, uint32_t stackCount = 6);
	GeometryData CreateWireCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

	GeometryData GenerateCubeGrid(int rows, int cols, int layers);
	
}
