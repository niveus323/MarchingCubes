#include "pch.h"
#include "MeshGenerator.h"

namespace MeshGenerator
{
	using namespace DirectX;

	GeometryData CreateSphereMeshData(float radius, const DirectX::XMFLOAT4& color, uint32_t sliceCount, uint32_t stackCount)
	{
		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		result.vertices.push_back(Vertex{ 
			.pos = {0.0f, radius, 0.0f}, 
			.normal = {0.0f, 1.0f, 0.0f}, 
			.texCoord = {0.0f, 0.0f},
			.color = color 
		});

		for (uint32_t stack = 1; stack < stackCount; ++stack)
		{
			float phi = XM_PI * stack / stackCount;
			float v = (float)stack / stackCount; // UV.y
			for (uint32_t slice = 0; slice <= sliceCount; ++slice)
			{
				float u = (float)slice / sliceCount; // UV.x
				float theta = XM_2PI * slice / sliceCount;
				float x = radius * sinf(phi) * cosf(theta);
				float y = radius * cosf(phi);
				float z = radius * sinf(phi) * sinf(theta);

				Vertex vertex{
					.pos = {x,y,z},
					.texCoord = {u, v},
					.color = color
				};
				XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&vertex.pos));
				XMStoreFloat3(&vertex.normal, n);

				result.vertices.push_back(vertex);
			}
		}

		result.vertices.push_back(Vertex{ 
			.pos = {0.0f, -radius, 0.0f}, 
			.normal = {0.0f, -1.0f, 0.0f},
			.texCoord = {0.0f, 1.0f},
			.color = color 
		});

		for (uint32_t i = 1; i <= sliceCount; ++i)
		{
			result.indices.push_back(0);
			result.indices.push_back(i);
			result.indices.push_back(i + 1);
		}

		uint32_t ringVertexCount = sliceCount + 1;
		for (uint32_t stack = 0; stack < stackCount - 2; ++stack)
		{
			for (uint32_t slice = 0; slice < sliceCount; ++slice)
			{
				uint32_t i0 = 1 + stack * ringVertexCount + slice;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = i0 + ringVertexCount;
				uint32_t i3 = i2 + 1;

				result.indices.push_back(i0);
				result.indices.push_back(i2);
				result.indices.push_back(i1);

				result.indices.push_back(i1);
				result.indices.push_back(i2);
				result.indices.push_back(i3);
			}
		}

		uint32_t southPoleIndex = (uint32_t)result.vertices.size() - 1;
		uint32_t baseIndex = 1 + (stackCount - 2) * ringVertexCount;
		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			result.indices.push_back(southPoleIndex);
			result.indices.push_back(baseIndex + i + 1);
			result.indices.push_back(baseIndex + i);
		}

		return result;
	}

	GeometryData GenerateCubeGrid(int rows, int cols, int layers)
	{
		int X = rows + 1;
		int Y = cols + 1;
		int Z = layers + 1;

		GeometryData result;
		result.topology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;

		result.vertices.clear();
		result.indices.clear();
		result.vertices.reserve(static_cast<size_t>(X * Y * Z));
		result.indices.reserve(static_cast<size_t>(((X - 1) * Y * Z + X * (Y - 1) * Z + X * Y * (Z - 1)) * 2));

		auto pushMCVertex = [&](int gx, int gy, int gz) {
			Vertex v{
			 .pos = { float(gx), float(gy), float(gz) },
			 .color = { 1,1,1,1 }
			};
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.pos));
			XMStoreFloat3(&v.normal, n);
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