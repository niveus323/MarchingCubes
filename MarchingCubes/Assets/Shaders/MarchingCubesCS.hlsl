// MarchingCubesCS.hlsl
static const int cornerIndexAFromEdge[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
static const int cornerIndexBFromEdge[12] = { 1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7 };

cbuffer GridCB : register(b0)
{
    uint3 gridCells; 
    uint chunkID;
    
    float3 gridOrigin;
    float isoValue;
    
    uint3 numChunkAxis;
    uint chunkCubes; // 16
    
    uint3 regionCellMin;
    uint _padding1;
    
    uint3 regionCellMax;
    uint _padding2;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent; // xyz: tangent, w: handedness(+1)
    float2 texcoord;
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

AppendStructuredBuffer<Triangle> gTriangles : register(u0);

// Chunk 기준 고유 인덱스 <  1차원 벡터 -> [z][y][x] 인덱싱  >
int IndexFromCoord(int3 coord)
{
    return coord.z * (gridCells.x * gridCells.y) + coord.y * gridCells.x + coord.x;
}

// 밀도 샘플링 ( 경계 클램프 )
float SampleDensity(int3 coord)
{
    int3 lo = int3(0, 0, 0);
    int3 hi = int3(gridCells) - int3(1, 1, 1);
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
    return (dot(n, n) > 1e-20) ? -normalize(n) : float3(0, 1, 0);
}

float2 CalculateBoxUV(float3 position, float3 normal)
{
    // 텍스처 타일링 스케일 (값이 클수록 텍스처가 촘촘해짐)
    const float texScale = 0.05f;

    float3 p = position * texScale;
    float3 absN = abs(normal);

    // 노말의 성분이 가장 큰 축을 기준으로 투영 면 결정
    if (absN.x > absN.y && absN.x > absN.z)
    {
        // X축이 주축 -> 옆면 (YZ 평면 투영)
        return p.yz;
    }
    else if (absN.z > absN.y)
    {
        // Z축이 주축 -> 앞/뒷면 (XY 평면 투영)
        return p.xy;
    }
    else
    {
        // Y축이 주축 -> 윗면/바닥 (XZ 평면 투영)
        return p.xz;
    }
}

float4 CalculateBoxTangent(float3 normal)
{
    float3 absN = abs(normal);
    float3 tempTangent;

    // UV 생성 로직과 "U 좌표"의 진행 방향을 맞춰야 함
    if (absN.x > absN.y && absN.x > absN.z)
    {
        // X축 주축 (YZ 평면 투영) -> UV가 p.yz 이므로 U는 y축, V는 z축
        // 따라서 탄젠트(U방향)는 (0, 1, 0) 이어야 함 (혹은 회전 여부에 따라 0,0,1)
        // 여기서는 일반적인 매핑 기준인 Z축 진행을 가정
        tempTangent = float3(0.0f, 0.0f, 1.0f);
    }
    else if (absN.z > absN.y)
    {
        // Z축 주축 (XY 평면 투영) -> UV가 p.xy
        // U는 x축
        tempTangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        // Y축 주축 (XZ 평면 투영) -> UV가 p.xz
        // U는 x축
        tempTangent = float3(1.0f, 0.0f, 0.0f);
    }

    // Gram-Schmidt 직교화: 노멀과 수직이 되도록 보정
    // tangent = normalize(temp - normal * dot(temp, normal));
    return float4(normalize(tempTangent - normal * dot(tempTangent, normal)), 1.0f);
}

// Edge 보간 정점 생성
Vertex CreateVertex(int3 coordA, int3 coordB)
{
    Vertex v;
    float dA = SampleDensity(coordA);
    float dB = SampleDensity(coordB);
    
    float denom = (dB - dA);
    float t = (abs(denom) > 1e-8) ? (isoValue - dA) / denom : 0.5;
 
    float3 posA = float3(coordA + gridOrigin);
    float3 posB = float3(coordB + gridOrigin);
    v.position = lerp(posA, posB, saturate(t));
    
    float3 nA = CalculateNormal(coordA);
    float3 nB = CalculateNormal(coordB);
    v.normal = normalize(lerp(nA, nB, saturate(t)));
    
    v.texcoord = CalculateBoxUV(v.position, v.normal);
    
    v.tangent = CalculateBoxTangent(v.normal);
    
    return v;
}

[numthreads(8, 8, 8)]
void MCMainCS(uint3 DTid : SV_DispatchThreadID)
{   
    int3 localPos = int3(DTid) + int3(1, 1, 1);
    // 범위 밖이면 return
    if (any(DTid >= chunkCubes)) return;
    
    // 현재 Edge를 원점으로 하는 큐브를 형성
    int3 c[8];
    c[0] = localPos + int3(0, 0, 0);
    c[1] = localPos + int3(1, 0, 0);
    c[2] = localPos + int3(1, 0, 1);
    c[3] = localPos + int3(0, 0, 1);
    c[4] = localPos + int3(0, 1, 0);
    c[5] = localPos + int3(1, 1, 0);
    c[6] = localPos + int3(1, 1, 1);
    c[7] = localPos + int3(0, 1, 1);

    // Marching Cubes 알고리즘 적용
    int cfg = 0;
    int i = 0;
    [unroll]
    for (i = 0; i < 8; ++i)
    {
        // T/F 구분
        if(SampleDensity(c[i]) < isoValue)
            cfg |= (1u << i);
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
        tri.chunkIdx = chunkID;
        gTriangles.Append(tri);
    }
}