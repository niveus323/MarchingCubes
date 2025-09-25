// BrushCS.hlsl

cbuffer BrushCB : register(b0)
{
    int resolution; // 3D SDF의 해상도 (RWTexture 한 변의 포인트 개수)
    float radius; // Brush 반경
    float deltaTime;
    float weight; // 가중치

    uint3 gridDim;
    int _padding0;
    
    uint3 brushcenter;
    int _padding1;
    
    uint3 regionMin;
    int _padding2;
    uint3 regionMax;
    int _padding3;
}

RWTexture3D<float> editTexture : register(u1);

[numthreads(8,8,8)]
void BrushCS(uint3 gid : SV_DispatchThreadID)
{   
    int3 center = brushcenter;
    uint3 p = regionMin + gid;
    
    if (any(p >= regionMax) || any(p >= gridDim))
        return;

    float d = distance(float3(p), float3(center));
    if (d > radius)
        return;
    
    float t = 1.0 - (d / radius);
    float w = smoothstep(0.0, 1.0, t);
    editTexture[p] += weight * deltaTime * w;
}