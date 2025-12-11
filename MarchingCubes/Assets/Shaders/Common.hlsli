// --------------------------------------
// Common.hlsli
// - 상수 및 cbuffer,Light 구조체 정의.
// --------------------------------------
#ifndef COMMON_HLSLI
#define COMMON_HLSLI

cbuffer CameraBuffer : register(b0)
{
    matrix gViewProj; // View-Projection matrix
    float3 gCameraPos; // world-space camera position
    float _padding_cam0;
};

cbuffer ObjectBuffer : register(b1)
{
    matrix gWorld;
    matrix gWorldInv;
    uint gMaterialIndex;
};

struct ELightType
{
    static const uint DIRECTIONAL = 0;
    static const uint POINT = 1;
    static const uint SPOT = 1;
};

struct Light
{
    uint type;
    uint3 _padding_Light0;
    float3 radiance;
    float _padding_Light1;
    
    float3 param0; // direction or position
    float rangeOrPad;
    float3 spotParam1; // spot.direction
    float spotParam2;
};

cbuffer LightBuffer : register(b2)
{
    uint g_NumLights;
    uint3 padding;
    Light g_Lights[128];
};
#endif // COMMON_HLSLI