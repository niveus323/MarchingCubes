// --------------------------------------
// Texture.hlsli
// - Texture, Sampler 정의 및 헬퍼 함수
// --------------------------------------
#ifndef TEXTURE_HLSLI
#define TEXTURE_HLSLI

// Register
TextureCube gEnvMap : register(t1);
Texture2D gMaterialTextures[] : register(t2); // Bindless
SamplerState gLinearSampler : register(s0);

// Texture
static const uint INVALID_TEXTURE_INDEX = 0xFFFFFFFFu;

float3 SampleTriplanar(Texture2D texArray, SamplerState samp, float3 worldPos, float3 normal, float scale, float sharpness)
{
    float3 n = normalize(normal);
    float3 an = abs(n);
    
    float2 uvX = worldPos.yz * scale;
    float2 uvY = worldPos.xz * scale;
    float2 uvZ = worldPos.xy * scale;
    
    float3 xTex = texArray.Sample(samp, uvX).rgb;
    float3 yTex = texArray.Sample(samp, uvY).rgb;
    float3 zTex = texArray.Sample(samp, uvZ).rgb;
    
    float3 w = pow(an, sharpness); // an^sharpness
    float sum = w.x + w.y + w.z + 1e-5;
    w /= sum;

    return xTex * w.x + yTex * w.y + zTex * w.z;
}
#endif //TEXTURE_HLSLI