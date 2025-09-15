// MarchingCubesCS.hlsl
static const int cornerIndexAFromEdge[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
static const int cornerIndexBFromEdge[12] = { 1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7 };

cbuffer GridCB : register(b0)
{
    uint3 cells; // 격자 개수 (x,y,z축이 동일하다고 가정하였음)
    float scale; // 정육면체 격자 1개의 한 변 크기
    float3 origin;
    float isoValue;
    uint3 numChunkAxis;
    uint chunkCubes; // 16
    uint3 gropusPerChunk; // (chunkCubes + 7) / 8
    uint chunkCapacity;
};
struct Vertex
{
    float3 position;
    float3 normal;
    int2 id; // 이 정점이 어떤 Edge 사이의 정점인지를 표현. (A 포인트 인덱스 - B 포인트 인덱스) 
}; 

struct Triangle 
{ 
    Vertex vertexC; 
    Vertex vertexB; 
    Vertex vertexA; 
    uint chunkIdx;
};

StructuredBuffer<int> gTritable : register(t0);
Texture3D<float> gDensityTex : register(t1);
StructuredBuffer<uint> gChunkMask : register(t3);

//AppendStructuredBuffer<Triangle> gTriangles : register(u0);
RWStructuredBuffer<Triangle> gTriangles : register(u0);
RWStructuredBuffer<uint2> gChunkMeta : register(u6); // x : touched, y : counter

// Chunk 기준 고유 인덱스 <  1차원 벡터 -> [z][y][x] 인덱싱  >
int IndexFromCoord(int3 coord)
{
    return coord.z * (cells.x * cells.y) + coord.y * cells.x + coord.x;
}

// 밀도 샘플링 ( 경계 클램프 )
float SampleDensity(int3 coord)
{
    int3 lo = int3(0, 0, 0);
    int3 hi = int3(cells) - int3(1, 1, 1);
    int3 c = clamp(coord, lo, hi);
    return gDensityTex.Load(int4(c, 0));
}

float3 CalculateNormal(int3 coord)
{
    const int3 ox = int3(1, 0, 0);
    const int3 oy = int3(0, 1, 0);
    const int3 oz = int3(0, 0, 1);
    
    float dx = SampleDensity(coord + ox) - SampleDensity(coord - ox);
    float dy = SampleDensity(coord + oy) - SampleDensity(coord - oy);
    float dz = SampleDensity(coord + oz) - SampleDensity(coord - oz);
    
    float3 n = float3(dx, dy, dz);
    return (dot(n, n) > 1e-20) ? normalize(n) : float3(0, 1, 0);
}

// Edge 보간 정점 생성
Vertex CreateVertex(int3 coordA, int3 coordB)
{
    float dA = SampleDensity(coordA);
    float dB = SampleDensity(coordB);
    
    float denom = (dB - dA);
    float t = (abs(denom) > 1e-8) ? (isoValue - dA) / denom : 0.5;
 
    float3 posA = float3(coordA + origin);
    float3 posB = float3(coordB + origin);
    float3 position = lerp(posA, posB, saturate(t));
    
    float3 nA = CalculateNormal(coordA);
    float3 nB = CalculateNormal(coordB);
    float3 normal = normalize(lerp(nA, nB, saturate(t)));
    
    // Edge ID
    int idxA = IndexFromCoord(coordA);
    int idxB = IndexFromCoord(coordB);
    
    Vertex v;
    v.position = position;
    v.normal = normal;
    v.id = int2(min(idxA, idxB), max(idxA, idxB));
    
    return v;
}

[numthreads(8, 8, 8)]
void MCMainCS(uint3 groupId : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint3 chunkgroup = groupId / gropusPerChunk;
    uint3 loc = groupId % gropusPerChunk;
    
    // 청크 바깥 공간은 계산 x
    if (any(chunkgroup >= numChunkAxis)) return;
    
    // Dirty Mask 검사
    uint chunkIdx = (chunkgroup.z * numChunkAxis.y + chunkgroup.y) * numChunkAxis.x + chunkgroup.x;
    uint word = chunkIdx >> 5;
    uint bit = 1u << (chunkIdx & 31u);
    if ((gChunkMask[word] & bit) == 0)
        return;
    
    // Chunk 샘플 AABB
    uint3 chunkMinS = chunkgroup * chunkCubes;
    uint3 chunkMaxS = min(chunkMinS + chunkCubes, cells);
    
    uint3 cubeExt = (chunkMaxS - chunkMinS) - uint3(1, 1, 1);
    if (any(cubeExt == int3(0,0,0)))
        return;
    
    // 그룹의 base 좌표
    uint3 base = chunkMinS + loc * 8 + groupThreadID;
    uint3 limit = min(chunkMaxS, cells - uint3(1, 1, 1));
    // 그룹이 청크 경계를 벗어나면 패스
    if (any(base >= limit))
        return;
    
    // 현재 Edge를 원점으로 하는 큐브를 형성
    int3 c[8];
    c[0] = base + int3(0, 0, 0);
    c[1] = base + int3(1, 0, 0);
    c[2] = base + int3(1, 0, 1);
    c[3] = base + int3(0, 0, 1);
    c[4] = base + int3(0, 1, 0);
    c[5] = base + int3(1, 1, 0);
    c[6] = base + int3(1, 1, 1);
    c[7] = base + int3(0, 1, 1);

    // Marching Cubes 알고리즘 적용
    int cfg = 0;
    int i = 0;
    [unroll]
    for (i = 0; i < 8; ++i)
    {
        // T/F 구분
        if(SampleDensity(c[i]) < isoValue)
            cfg |= (1 << i);
    }
    
    [unroll]
    for (i = 0; i < 16; i+=3)
    {
        int eA = gTritable[cfg * 16 + i];
        if (eA == -1)
            break;
        
        int eB = gTritable[cfg * 16 + i + 1];
        int eC = gTritable[cfg * 16 + i + 2];
        
        int a0 = cornerIndexAFromEdge[eA];
        int a1 = cornerIndexBFromEdge[eA];

        int b0 = cornerIndexAFromEdge[eB];
        int b1 = cornerIndexBFromEdge[eB];

        int c0 = cornerIndexAFromEdge[eC];
        int c1 = cornerIndexBFromEdge[eC];

        Vertex vA = CreateVertex(c[a0], c[a1]);
        Vertex vB = CreateVertex(c[b0], c[b1]);
        Vertex vC = CreateVertex(c[c0], c[c1]);

        Triangle tri;
        tri.vertexA = vC;
        tri.vertexB = vB;
        tri.vertexC = vA;
        tri.chunkIdx = chunkIdx;
        
        uint localIdx = 0;
        InterlockedAdd(gChunkMeta[chunkIdx].y, 1, localIdx);
        
        if (localIdx >= chunkCapacity)
            return;
        
        uint globalIdx = chunkIdx * chunkCapacity + localIdx;
        gTriangles[globalIdx] = tri;
        //gTriangles.Append(tri);
    }
}