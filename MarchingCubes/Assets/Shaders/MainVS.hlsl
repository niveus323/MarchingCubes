// shaders.hlsl
#include "Common.hlsli"

// Vertex input structure
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;       // For Debugging
};

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION0;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float4 Color : COLOR0;
};

// Vertex Shader: transform position by view-projection
PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 modelPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(modelPos, gWorld);
    output.Position = mul(worldPos, gViewProj);
    output.WorldPos = worldPos.xyz;
    float3x3 worldInvT = transpose((float3x3) gWorldInv);
    output.WorldNormal = normalize(mul(input.Normal, worldInvT));
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}
