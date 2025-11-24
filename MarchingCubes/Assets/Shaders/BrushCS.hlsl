// BrushCS.hlsl

cbuffer BrushCB : register(b0)
{
    float brushRadius;
    float brushWeight;
    float deltaTime;
    float _padding0;

    uint3 gridCells;
    int _padding1;
    
    uint3 brushcenter;
    int _padding2;
    
    uint3 regionCellMin;
    int _padding3;
    uint3 regionCellMax;
    int _padding4;
}

RWTexture3D<float> editTexture : register(u1);

[numthreads(8,8,8)]
void BrushCS(uint3 gid : SV_DispatchThreadID)
{   
    int3 center = brushcenter;
    uint3 p = regionCellMin + gid;
    
    if (any(p >= regionCellMax) || any(p >= gridCells))
        return;

    float d = distance(float3(p), float3(center));
    if (d > brushRadius)
        return;
    
    float t = 1.0 - (d / brushRadius);
    float w = smoothstep(0.0, 1.0, t);
    editTexture[p] += brushWeight * deltaTime * w;
}