// --------------------------------------
// TerrainChunkBorederGS.hlsl
// - 지형 편집 디버깅용 청크 경계 메쉬 생성
// --------------------------------------
#include "Common.hlsli"
#define FLT_EPSILON 0.0001f

cbuffer ChunkBoundaryBuffer : register(b3)
{
    float3 g_GridOrigin;     // 지형 원점
    float _pad0;
    float3 g_ChunkWorldSize; // 청크 크기 (chunksize * cellsize)
    float g_ViewBias;        // 오프셋용 Bias
    float4 g_LineColor;      // 라인 색상
};
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

bool IsOnBoundary(float val, float origin, float interval)
{
    float localVal = val - origin;
    
    if (interval < 0.001f) return false;
    
    float index = round(localVal / interval); // 가장 가까운 정수
    float dist = abs(localVal - (index * interval));
    
    return dist < FLT_EPSILON;
}

[maxvertexcount(6)]
void GSMain(triangle PSInput input[3], inout LineStream<PSInput> outputStream)
{
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        uint idx0 = i;
        uint idx1 = (i + 1) % 3;

        float3 p0World = input[idx0].WorldPos;
        float3 p1World = input[idx1].WorldPos;
        
        float3 p0Local = mul(float4(p0World, 1.0f), gWorldInv).xyz;
        float3 p1Local = mul(float4(p1World, 1.0f), gWorldInv).xyz;
        
        // 경계 검사
        bool bOnX = IsOnBoundary(p0Local.x, g_GridOrigin.x, g_ChunkWorldSize.x) && IsOnBoundary(p1Local.x, g_GridOrigin.x, g_ChunkWorldSize.x);
        bool bOnZ = IsOnBoundary(p0Local.z, g_GridOrigin.z, g_ChunkWorldSize.z) && IsOnBoundary(p1Local.z, g_GridOrigin.z, g_ChunkWorldSize.z);
        if (bOnX || bOnZ)
        {
            PSInput v0 = input[idx0];
            PSInput v1 = input[idx1];
            v0.Color = g_LineColor;
            v1.Color = g_LineColor;

            // Z-Fighting 방지
            float3 biasedPos0 = v0.WorldPos + (v0.WorldNormal * g_ViewBias);
            float3 biasedPos1 = v1.WorldPos + (v1.WorldNormal * g_ViewBias);
            
            // Bias를 적용해서 Position 조정
            v0.Position = mul(float4(biasedPos0, 1.0f), gViewProj);
            v1.Position = mul(float4(biasedPos1, 1.0f), gViewProj);
            outputStream.Append(v0);
            outputStream.Append(v1);
            outputStream.RestartStrip();
        }
    }
}