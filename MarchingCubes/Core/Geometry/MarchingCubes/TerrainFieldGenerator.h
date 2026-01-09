#pragma once
#include "SdfField.h"

template <typename T>
class TerrainFieldGenerator
{
public:
	static std::shared_ptr<SdfField<T>> CreateFromImage(const GridDesc& desc, const std::vector<uint8_t>& pixelData, int imgWidth, int imgHeight, float heightScale)
	{
        if (desc.cells.x < 1 || desc.cells.y < 1 || desc.cells.z < 1 ||
            pixelData.empty() || imgWidth <= 0 || imgHeight <= 0) 
            return nullptr;

        const int SX = desc.cells.x + 1;
        const int SY = desc.cells.y + 1;
        const int SZ = desc.cells.z + 1;
        auto gridData = new SdfField<T>(SX, SY, SZ);
        float invCellSize = 1.0f / desc.cellsize;

        // 3D 루프
        for (int z = 0; z < SZ; ++z) {
            for (int x = 0; x < SX; ++x) {
                // 이미지 좌표 계산
                // TODO : BilinearInterpolation 적용
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
                    gridData->at(x, y, z) = std::clamp(dist, -1.0f, 1.0f);
                }
            }
        }
        return std::shared_ptr<SdfField<T>>(gridData);
	}
};