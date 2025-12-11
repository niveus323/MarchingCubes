// LineShaders.hlsl
#include "Common.hlsli"

//개발 편의를 위해 VertexShader는 shaders.hlsl의 VSMain을 사용

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION0;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
    float TangentSign : TEXCOORD4;
    float4 Color : COLOR0;
};

// Pixel Shader: output interpolated color
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.Color;
}
