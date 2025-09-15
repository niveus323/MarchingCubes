#include "pch.h"
#include "MeshGenerator.h"

namespace MeshGenerator
{
	using namespace DirectX;

	MeshData CreateSphereMeshData(float radius, const DirectX::XMFLOAT4& color, UINT sliceCount, UINT stackCount)
	{
		MeshData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		result.vertices.push_back({ {0.0f, radius, 0.0f}, {0.0f, 1.0f, 0.0f}, color });

		for (UINT stack = 1; stack < stackCount; ++stack)
		{
			float phi = XM_PI * stack / stackCount;
			for (UINT slice = 0; slice <= sliceCount; ++slice)
			{
				float theta = XM_2PI * slice / sliceCount;
				float x = radius * sinf(phi) * cosf(theta);
				float y = radius * cosf(phi);
				float z = radius * sinf(phi) * sinf(theta);

				Vertex v;
				v.pos = { x,y,z };
				XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.pos));
				XMStoreFloat3(&v.normal, n);
				v.color = color;

				result.vertices.push_back(v);
			}
		}

		result.vertices.push_back({ {0.0f, -radius, 0.0f}, {0.0f, -1.0f, 0.0f}, color });

		for (UINT i = 1; i <= sliceCount; ++i)
		{
			result.indices.push_back(0);
			result.indices.push_back(i);
			result.indices.push_back(i + 1);
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

				result.indices.push_back(i0);
				result.indices.push_back(i2);
				result.indices.push_back(i1);

				result.indices.push_back(i1);
				result.indices.push_back(i2);
				result.indices.push_back(i3);
			}
		}

		UINT southPoleIndex = (UINT)result.vertices.size() - 1;
		UINT baseIndex = 1 + (stackCount - 2) * ringVertexCount;
		for (UINT i = 0; i < sliceCount; ++i)
		{
			result.indices.push_back(southPoleIndex);
			result.indices.push_back(baseIndex + i + 1);
			result.indices.push_back(baseIndex + i);
		}

		return result;
	}

	MeshData GenerateCubeGrid(int rows, int cols, int layers)
	{
		int X = rows + 1;
		int Y = cols + 1;
		int Z = layers + 1;

		MeshData result;
		result.topology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;

		result.vertices.clear();
		result.indices.clear();
		result.vertices.reserve(X * Y * Z);
		result.indices.reserve(((X - 1) * Y * Z + X * (Y - 1) * Z + X * Y * (Z - 1)) * 2);

		auto pushMCVertex = [&](int gx, int gy, int gz) {
			Vertex v{};
			v.pos = { float(gx), float(gy), float(gz) };
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.pos));
			XMStoreFloat3(&v.normal, n);
			v.color = { 1,1,1,1 };
			result.vertices.push_back(v);
		};
		
		auto idx = [&](int i, int j, int k) {
			return i * (Y * Z) + j * Z + k;
		};

		for (int i = 0; i < X; ++i)
		{
			for (int j = 0; j < Y; ++j)
			{
				for (int k = 0; k < Z; ++k)
				{
					// ÀÌ ¼¿ÀÇ Ã¹ Á¤Á¡ ÀÎµ¦½º
					int base = idx(i,j,k);
					pushMCVertex(i, j, k);
					
					if (i + 1 < X)
					{
						result.indices.push_back(base);
						result.indices.push_back(idx(i + 1, j, k));
					}

					if (j + 1 < Y)
					{
						result.indices.push_back(base);
						result.indices.push_back(idx(i, j + 1, k));
					}
					
					if (k + 1 < Z)
					{
						result.indices.push_back(base);
						result.indices.push_back(idx(i, j, k + 1));
					}
				}
			}
		}
		return result;
	}
}