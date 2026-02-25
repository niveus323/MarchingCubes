// LineShaders.hlsl
#include "Common.hlsli"
#include "Material.hlsli"
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

// Entry Point 1: Line/Wireframe 등 Material을 적용
float4 PSMain_Material(PSInput input) : SV_TARGET
{
    // Material이 없을 경우 디폴트로 빨간색
    if (gMaterialIndex == INVALID_MATERIAL_INDEX)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f); // Error Color
    }

    // Albedo 색상만 추출
    return float4(gMaterials[gMaterialIndex].albedo, 1.0f);
}

// Entry Point 2: 지정한 Color 값으로 그려야 하는 경우 (노말 벡터 시각화 등)
float4 PSMain_Visualizer(PSInput input) : SV_TARGET
{
    return input.Color;
}