#include "Common.hlsli"

struct PSInput
{
    float4 Position : SV_POSITION0;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float4 Color : COLOR0;
};

[maxvertexcount(6)]
void GSMain(triangle PSInput tri[3], inout LineStream<PSInput> outStream)
{
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        PSInput v0 = tri[i];
        v0.Color = float4(0.0f, 1.0f, 0.0f, 1.0f);

        // 월드에서 노말 방향으로 gNormalLength만큼 이동
        float3 endWorld = tri[i].WorldPos + normalize(tri[i].WorldNormal) * 1.0f;

        // clip-space로 변환
        float4 endClip = mul(float4(endWorld, 1.0f), gViewProj);

        PSInput v1 = v0;
        v1.Position = endClip;
        v1.TexCoord = float2(1.0f, 1.0f);
        v1.Color = float4(1.0f, 0.0f, 0.0f, 1.0f);
        
        outStream.Append(v0);
        outStream.Append(v1);
        outStream.RestartStrip();
    }
}