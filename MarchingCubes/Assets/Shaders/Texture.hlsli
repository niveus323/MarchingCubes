// --------------------------------------
// Texture.hlsli
// - Texture, Sampler 정의 및 헬퍼 함수
// --------------------------------------
#ifndef TEXTURE_HLSLI
#define TEXTURE_HLSLI

struct ETextureMappingType
{
    static const uint DEFAULT_UV = 0;
    static const uint TRIPLANAR = 1;
    static const uint SPHERICAL = 2;
};

// Register
TextureCube gEnvMap : register(t1);
Texture2D gMaterialTextures[] : register(t2); // Bindless
SamplerState gLinearSampler : register(s0);

// Texture
static const uint INVALID_TEXTURE_INDEX = 0xFFFFFFFFu;

float3 SampleTriplanar(Texture2D tex, SamplerState samp, float3 worldPos, float3 normal, float scale, float sharpness)
{
    float3 n = normalize(normal);
    float3 an = abs(n);
    
    float2 uvX = worldPos.yz * scale;
    float2 uvY = worldPos.xz * scale;
    float2 uvZ = worldPos.xy * scale;
    
    float3 xTex = tex.Sample(samp, uvX).rgb;
    float3 yTex = tex.Sample(samp, uvY).rgb;
    float3 zTex = tex.Sample(samp, uvZ).rgb;
    
    float3 w = pow(an, sharpness); // an^sharpness
    float sum = w.x + w.y + w.z + 1e-5;
    w /= sum;

    return xTex * w.x + yTex * w.y + zTex * w.z;
}

float3 SampleFromSet(uint index, uint mappingType, float scale, float sharpness, float2 uv, float3 worldPos, float3 normal)
{
    if (index == INVALID_TEXTURE_INDEX)
        return 1.0.xxx;
    
    if (mappingType == ETextureMappingType::TRIPLANAR)
    {
        return SampleTriplanar(gMaterialTextures[index], gLinearSampler, worldPos, normal, scale, sharpness);
    }
    else
    {
        // default UV mapping
        return gMaterialTextures[index].Sample(gLinearSampler, uv).rgb;
    }
}

#endif //TEXTURE_HLSLI