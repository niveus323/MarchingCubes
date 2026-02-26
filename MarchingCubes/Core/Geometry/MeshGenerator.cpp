#include "pch.h"
#include "MeshGenerator.h"

namespace MeshGenerator
{
	using namespace DirectX;

	GeometryData CreateSphereMeshData(float radius, uint32_t sliceCount, uint32_t stackCount)
	{
		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		result.vertices.push_back(Vertex{
			.pos = {0.0f, radius, 0.0f},
			.normal = {0.0f, 1.0f, 0.0f},
			.tangent = {1.0f, 0.0f, 0.0f, 1.0f},
			.texCoord = {0.0f, 0.0f}
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
					.texCoord = {u, v}
				};

				XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&vertex.pos));
				XMStoreFloat3(&vertex.normal, n);

				float tx = -radius * sinf(phi) * sinf(theta);
				float ty = 0.0f;
				float tz = radius * sinf(phi) * cosf(theta);

				XMVECTOR t = XMVector3Normalize(XMVectorSet(tx, ty, tz, 0.0f));
				XMFLOAT3 tan;
				XMStoreFloat3(&tan, t);

				vertex.tangent = { tan.x, tan.y, tan.z, 1.0f };

				result.vertices.push_back(vertex);
			}
		}

		result.vertices.push_back(Vertex{
			.pos = {0.0f, -radius, 0.0f},
			.normal = {0.0f, -1.0f, 0.0f},
			.tangent = {1.0f, 0.0f, 0.0f, 1.0f},
			.texCoord = {0.0f, 1.0f}
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

	GeometryData CreateSolidCube(float width, float height, float depth)
	{
		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		float w2 = width * 0.5f;
		float h2 = height * 0.5f;
		float d2 = depth * 0.5f;

		// Face Á¤ÀÇ (Normal, Tangent, UÃà, VÃà)
		struct FaceInfo {
			XMFLOAT3 normal;
			XMFLOAT4 tangent;
		};

		FaceInfo faces[6] = {
			{ {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }, // Front
			{ {0.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, 0.0f, 1.0f} }, // Back
			{ {0.0f, 1.0f, 0.0f},  {1.0f, 0.0f, 0.0f, 1.0f} }, // Top
			{ {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }, // Bottom
			{ {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f} }, // Left
			{ {1.0f, 0.0f, 0.0f},  {0.0f, 0.0f, -1.0f, 1.0f} }  // Right
		};

		// ¼ø¼­: Top-Left, Top-Right, Bottom-Right, Bottom-Left (Looking at the face)
		XMFLOAT3 faceVerts[6][4] = {
			{ {-w2, h2, -d2}, {w2, h2, -d2}, {w2, -h2, -d2}, {-w2, -h2, -d2} }, // Front
			{ {w2, h2, d2}, {-w2, h2, d2}, {-w2, -h2, d2}, {w2, -h2, d2} },    // Back
			{ {-w2, h2, d2}, {w2, h2, d2}, {w2, h2, -d2}, {-w2, h2, -d2} },    // Top
			{ {-w2, -h2, -d2}, {w2, -h2, -d2}, {w2, -h2, d2}, {-w2, -h2, d2} }, // Bottom
			{ {-w2, h2, d2}, {-w2, h2, -d2}, {-w2, -h2, -d2}, {-w2, -h2, d2} }, // Left
			{ {w2, h2, -d2}, {w2, h2, d2}, {w2, -h2, d2}, {w2, -h2, -d2} }     // Right
		};

		XMFLOAT2 uvs[4] = { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} };

		for (int i = 0; i < 6; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				result.vertices.push_back(Vertex{
					.pos = faceVerts[i][j],
					.normal = faces[i].normal,
					.tangent = faces[i].tangent,
					.texCoord = uvs[j]
				});
			}

			uint32_t base = i * 4;
			result.indices.push_back(base + 0);
			result.indices.push_back(base + 1);
			result.indices.push_back(base + 2);

			result.indices.push_back(base + 0);
			result.indices.push_back(base + 2);
			result.indices.push_back(base + 3);
		}

		return result;
	}

	GeometryData CreateWireSphere(float radius, uint32_t sliceCount, uint32_t stackCount)
	{
		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

		// ºÏ±Ø
		result.vertices.push_back(Vertex{ .pos = {0.0f, radius, 0.0f} });

		for (uint32_t stack = 1; stack < stackCount; ++stack)
		{
			float phi = XM_PI * stack / stackCount;
			for (uint32_t slice = 0; slice <= sliceCount; ++slice)
			{
				float theta = XM_2PI * slice / sliceCount;
				float x = radius * sinf(phi) * cosf(theta);
				float y = radius * cosf(phi);
				float z = radius * sinf(phi) * sinf(theta);

				result.vertices.push_back(Vertex{ .pos = {x, y, z} });
			}
		}
		// ³²±Ø
		result.vertices.push_back(Vertex{ .pos = {0.0f, -radius, 0.0f} });

		uint32_t northPoleIndex = 0;
		uint32_t southPoleIndex = (uint32_t)result.vertices.size() - 1;
		uint32_t ringVertexCount = sliceCount + 1;

		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			result.indices.push_back(northPoleIndex);
			result.indices.push_back(1 + i);
		}

		for (uint32_t stack = 0; stack < stackCount - 2; ++stack)
		{
			for (uint32_t slice = 0; slice < sliceCount; ++slice)
			{
				uint32_t top = 1 + stack * ringVertexCount + slice;
				uint32_t bottom = top + ringVertexCount;
				result.indices.push_back(top);
				result.indices.push_back(bottom);
			}
		}

		uint32_t lastRingBase = 1 + (stackCount - 2) * ringVertexCount;
		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			result.indices.push_back(lastRingBase + i);
			result.indices.push_back(southPoleIndex);
		}

		for (uint32_t stack = 0; stack < stackCount - 1; ++stack)
		{
			uint32_t ringStart = 1 + stack * ringVertexCount;
			for (uint32_t slice = 0; slice < sliceCount; ++slice)
			{
				result.indices.push_back(ringStart + slice);
				result.indices.push_back(ringStart + slice + 1);
			}
		}

		return result;
	}

	GeometryData CreateWireCube(float width, float height, float depth)
	{
		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

		float w2 = width * 0.5f;
		float h2 = height * 0.5f;
		float d2 = depth * 0.5f;

		// 8°³ÀÇ ÄÚ³Ê Á¤Á¡
		XMFLOAT3 corners[8] = {
			{-w2, -h2, -d2}, // 0: Bottom-Left-Front
			{-w2,  h2, -d2}, // 1: Top-Left-Front
			{ w2,  h2, -d2}, // 2: Top-Right-Front
			{ w2, -h2, -d2}, // 3: Bottom-Right-Front
			{-w2, -h2,  d2}, // 4: Bottom-Left-Back
			{-w2,  h2,  d2}, // 5: Top-Left-Back
			{ w2,  h2,  d2}, // 6: Top-Right-Back
			{ w2, -h2,  d2}  // 7: Bottom-Right-Back
		};

		for (int i = 0; i < 8; ++i)
		{
			result.vertices.push_back(Vertex{ .pos = corners[i] });
		}

		uint32_t indices[] = {
			// Front Face
			0, 1, 1, 2, 2, 3, 3, 0,
			// Back Face
			4, 5, 5, 6, 6, 7, 7, 4,
			// Connecting Pillars
			0, 4, 1, 5, 2, 6, 3, 7
		};

		for (uint32_t idx : indices)
		{
			result.indices.push_back(idx);
		}

		return result;
	}

	GeometryData GenerateCubeGrid(int rows, int cols, int layers)
	{
		int X = rows + 1;
		int Y = cols + 1;
		int Z = layers + 1;

		GeometryData result;
		result.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		result.vertices.reserve(static_cast<size_t>(X * Y * Z));
		result.indices.reserve(static_cast<size_t>(((X - 1) * Y * Z + X * (Y - 1) * Z + X * Y * (Z - 1)) * 2));

		auto idx = [&](int i, int j, int k) {
			return i * (Y * Z) + j * Z + k;
		};

		// Vertex
		for (int i = 0; i < X; ++i)
		{
			for (int j = 0; j < Y; ++j)
			{
				for (int k = 0; k < Z; ++k)
				{
					Vertex v{
						.pos = { (float)i, (float)j, (float)k }
					};
					XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&v.pos));
					XMStoreFloat3(&v.normal, n);

					result.vertices.push_back(v);
				}
			}
		}

		// Index
		for (int i = 0; i < X; ++i)
		{
			for (int j = 0; j < Y; ++j)
			{
				for (int k = 0; k < Z; ++k)
				{
					int base = idx(i, j, k);

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