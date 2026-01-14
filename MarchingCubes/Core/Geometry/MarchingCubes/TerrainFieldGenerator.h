#pragma once
#include "SdfField.h"

class TerrainFieldGenerator
{
public:
	static std::shared_ptr<SdfField> CreateFromImage(const GridDesc& desc, const std::vector<uint8_t>& pixelData, int imgWidth, int imgHeight, float heightScale)
	{
        if (desc.resolution.x < 1 || desc.resolution.y < 1 || desc.resolution.z < 1 ||
            pixelData.empty() || imgWidth <= 0 || imgHeight <= 0) 
            return nullptr;

        const int SX = desc.resolution.x + 1;
        const int SY = desc.resolution.y + 1;
        const int SZ = desc.resolution.z + 1;
        auto gridData = new SdfField(SX, SY, SZ);
        float invCellSize = 1.0f / desc.cellsize;

        // 3D 루프
        for (int z = 0; z < SZ; ++z) {
            for (int x = 0; x < SX; ++x) {
                // 이미지 좌표 계산
                float u = (float)x / (float)(SX-1);
                float v = (float)z / (float)(SZ-1);
                int ix = std::clamp((int)(u * imgWidth), 0, imgWidth - 1);
                int iy = std::clamp((int)(v * imgHeight), 0, imgHeight - 1);

                // 픽셀 값(0~255)을 0.0~1.0으로 정규화 후 높이로 변환
                float pixelValue = pixelData[iy * imgWidth + ix] / 255.0f;
                float targetHeight = pixelValue * heightScale * invCellSize;

                // y축 순회하며 높이 값과 비교 후 값을 넣는다.
                for (int y = 0; y < SY; ++y) {
                    // SDF 밀도 계산
                    float dist = targetHeight - (float)y;
                    gridData->at(x, y, z) = static_cast<float>(std::clamp(dist, -2.0f, 2.0f)); // Narrow-Band : 2셀까지는 법선 계산을 위해 남겨둠
                }
            }
        }
        return std::shared_ptr<SdfField>(gridData);
	}

    static std::shared_ptr<SdfField> CreateSphere(const GridDesc& desc, float radius, DirectX::XMFLOAT3 center = {0.0f, 0.0f, 0.0f})
    {
        if (desc.resolution.x < 1 || desc.resolution.y < 1 || desc.resolution.z < 1) return nullptr;

        // 샘플 수 = (N+1)^3
        const int SX = desc.resolution.x + 1;
        const int SY = desc.resolution.y + 1;
        const int SZ = desc.resolution.z + 1;

        const float cellSize = desc.cellsize;
        const float invCellSize = 1.0f / cellSize;

        // 채우기: F = brushRadius - |p - center|
        auto gridData = new SdfField(SX, SY, SZ);
        for (int z = 0; z < SZ; ++z)
        {
            float worldZ = desc.origin.z + (z * cellSize);
            float dz = worldZ - center.z;

            for (int y = 0; y < SY; ++y)
            {
                float worldY = desc.origin.y + (y * cellSize);
                float dy = worldY - center.y;

                for (int x = 0; x < SX; ++x)
                {
                    float worldX = desc.origin.x + (x * cellSize);
                    float dx = worldX - center.x;

                    // 거리 계산 (World Unit)
                    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                    // SDF 값 계산 + 정규화 + (-1,1 클램핑)
                    float signedDist = radius - dist;
                    float normalizedDist = signedDist * invCellSize; // 이제부터 거리는 '셀 단위'가 된다.
                    gridData->at(x, y, z) = static_cast<float>(std::clamp(normalizedDist, -2.0f, 2.0f)); // Narrow-Band : 표면으로부터 2셀까지 판단
                }
            }
        }

        return std::shared_ptr<SdfField>(gridData);
    }

    static std::shared_ptr<SdfField> CreateEmpty(const GridDesc& desc, float isoValue = 0.0f)
    {
        if (desc.resolution.x < 1 || desc.resolution.y < 1 || desc.resolution.z < 1) return nullptr;

        // 샘플 수 = (N+1)^3
        const int SX = desc.resolution.x + 1;
        const int SY = desc.resolution.y + 1;
        const int SZ = desc.resolution.z + 1;

        auto gridData = new SdfField(SX, SY, SZ);
        int totalSize = SX * SY * SZ;
        const float defaultValue = isoValue - 1.0f;
        for (int i = 0; i < totalSize; ++i)
        {
            gridData->data()[i] = defaultValue;
        }

        return std::shared_ptr<SdfField>(gridData);

    }

    static std::shared_ptr<SdfField> CreatePlane(const GridDesc& desc, float height = 0.0f)
    {
        if (desc.resolution.x < 1 || desc.resolution.y < 1 || desc.resolution.z < 1) return nullptr;

        // 샘플 수 = (N+1)^3
        const int SX = desc.resolution.x + 1;
        const int SY = desc.resolution.y + 1;
        const int SZ = desc.resolution.z + 1;
        auto gridData = new SdfField(SX, SY, SZ);
        float invCellSize = 1.0f / desc.cellsize;
        for (int y = 0; y < SY; ++y)
        {
            float dist = height - static_cast<float>(y) * desc.cellsize + FLT_EPSILON;
            float val = std::clamp(dist * invCellSize, -2.0f, 2.0f);

            for (int z = 0; z < SZ; ++z)
                for (int x = 0; x < SX; ++x)
                {            
                    gridData->at(x, y, z) = val;
                }
        }

        return std::shared_ptr<SdfField>(gridData);
    }
};