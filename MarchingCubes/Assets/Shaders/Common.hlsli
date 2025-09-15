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
};

cbuffer MaterialBuffer : register(b2)
{
    float3 gAlbedo; // default color
    float gMetalic; // [0,1]
    
    float gSpecularStrength; // [0,1], default = 0.5
    float gRoughness; // [0,1]
    float gAmbientOcclusion; // [0,1]
    float gIOR; // Dielectric 전용
    
    uint gShadingModel; //0 - Default, 1 - Dielectric, 2 - Translucent    
    float gOpacity; // [0,1], Default - 1
    float2 _padding_Mat;
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

cbuffer LightBuffer : register(b3)
{
    uint g_NumLights;
    uint3 padding;
    Light g_Lights[128];
};

#endif // COMMON_HLSLI