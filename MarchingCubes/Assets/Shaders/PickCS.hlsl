// PickCS.hlsl
cbuffer PickCB : register(b0)
{
    float3 rayOriginG; // ray 원점 (grid space)
    float tMin; // 최소 거리 (grid space)
    float3 rayDirG; // ray 방향 (grid space, normalized)
    float tMax; // 최대 거리 (normalized in grid units)
    float isoValue;
    uint3 gridDim; // (Nx+1, Ny+1, Nz+1)
    uint maxSteps; // 안전장치 (예: 256~1024)
    float stepSize; // 예: 0.5f voxel
    float2 _padding;
};

Texture3D<float> DensityTex : register(t1);
RWStructuredBuffer<int4> OutPick : register(u2); // (x,y,z,valid)

[numthreads(1, 1, 1)]
void PickCS(uint3 tid : SV_DispatchThreadID)
{
    float t = tMin;
    float3 p = rayOriginG + rayDirG * t; // ray의 시작 지점
    int3 ip = clamp(int3(p), int3(0, 0, 0), int3(gridDim) - 1);
    float prev = DensityTex.Load(int4(ip, 0));
    bool hit = false;
    int3 hitG = int3(0, 0, 0);

    [loop]
    for (uint i = 0; i < maxSteps && t <= tMax; ++i)
    {
        p += rayDirG * stepSize;
        t += stepSize;
        int3 jp = clamp(int3(p), int3(0, 0, 0), int3(gridDim) - 1);
        float cur = DensityTex.Load(int4(jp, 0));

        if ((prev - isoValue) * (cur - isoValue) <= 0.0f)
        {
            float denom = (cur - prev);
            float alpha = (abs(denom) > 1e-6) ? saturate((isoValue - prev) / denom) : 0.5f;
            float3 phit = lerp(p - rayDirG * stepSize, p, alpha);
            hitG = clamp(int3(round(phit)), int3(0, 0, 0), int3(gridDim) - 1);
            hit = true;
            break;
        }
        prev = cur;
    }
    OutPick[0] = int4(hitG, hit ? 1 : 0);
}