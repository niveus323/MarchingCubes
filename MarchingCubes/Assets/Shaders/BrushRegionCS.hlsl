// BrushRegionCS.hlsl
cbuffer RegionArgsCB : register(b0)
{
    uint3 cells;     // 전체 grid 개수
    float cellSize;  // grid 크기
    
    float radius;    // Brush 반지름 (Grid Space)
    int halo;       // 경계 보정값(1)
    uint chunkCubes; // chunk당 grid의 개수
    int _padding0;
    
    uint3 brushcenter;
    int _padding1;
};

RWStructuredBuffer<uint4> OutRegion : register(u3);
RWStructuredBuffer<uint3> OutDispatchArgs : register(u4);
RWStructuredBuffer<uint> OutChunkMask : register(u5);
RWStructuredBuffer<uint2> OutChunkMeta : register(u6); // <touched, counter>

static const uint CHUNK_CUBES = 16;
static const uint META_STRIDE = 16; // NOTE : c++의 s_chunkcubes 값과 동일하게 맞출 것
static const uint META_OFF_PRED64 = 0; // uint2
static const uint META_OFF_COUNTER = 8;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint3 groups = uint3(1, 1, 1);
    
    int r = (int) ceil(radius / cellSize); // 피킹한 중심에서 radius *2 크기의 사각형 범위
    
    uint3 rmin = max(brushcenter - int3(r, r, r) + int3(halo, halo, halo), int3(0, 0, 0));
    uint3 rmax = min(brushcenter + int3(r, r, r) + int3(halo, halo, halo), int3(cells));
    
    // u3 결과
    OutRegion[0] = uint4(rmin, 0);
    OutRegion[1] = uint4(rmax, 0);
    
    // u4 결과
    uint3 ext = max(rmax - rmin, uint3(0,0,0));
    uint3 cubeExt = max(ext - uint3(1,1,1), uint3(0,0,0));
    
    if(any(cubeExt == uint3(0,0,0)))
    {
        OutDispatchArgs[0] = uint3(0, 0, 0);
        OutRegion[0] = uint4(0, 0, 0, 0);
        OutRegion[1] = uint4(0, 0, 0, 0);
        return;
    }
    
    uint3 threadgroup = (cubeExt + uint3(7,7,7)) / uint3(8,8,8);
    
    OutDispatchArgs[0] = max(threadgroup, uint3(1,1,1));
    
    // u5, u6 결과
    uint3 totalCubes = max(cells - 1, 0);
    uint3 numChunksAxis = (totalCubes + (CHUNK_CUBES - 1)) / CHUNK_CUBES;
    uint3 cubeMaxInclusive = clamp(rmax - 2, 0, int3(totalCubes) - 1);
    
    if (any(cubeMaxInclusive < rmin)) return;

    uint3 chunkMin = rmin / chunkCubes;
    uint3 chunkMax = cubeMaxInclusive / chunkCubes;
    
    [loop]
    for (uint z = chunkMin.z; z <= chunkMax.z; ++z)
    {
        [loop]
        for (uint y = chunkMin.y; y <= chunkMax.y; ++y)
        {
            [loop]
            for (uint x = chunkMin.x; x <= chunkMax.x; ++x)
            {
                uint idx = (z * numChunksAxis.y + y) * numChunksAxis.x + x;
                uint word   = idx >> 5;          // /32
                uint bitIdx = idx & 31u;         // %32
                uint bit    = 1u << bitIdx;

                InterlockedOr(OutChunkMask[word], bit);
                
                OutChunkMeta[idx] = uint2(1, 0);
            }
        }
    }
}