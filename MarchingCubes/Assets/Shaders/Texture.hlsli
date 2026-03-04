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

float4 SampleTriplanar(Texture2D tex, SamplerState samp, float3 localPos, float3 normal, float scale, float sharpness)
{
    float3 n = normalize(normal);
    float3 an = abs(n);
    
    float2 uvX = localPos.yz * scale;
    float2 uvY = localPos.xz * scale;
    float2 uvZ = localPos.xy * scale;
    
    float4 xTex = tex.Sample(samp, uvX);
    float4 yTex = tex.Sample(samp, uvY);
    float4 zTex = tex.Sample(samp, uvZ);
    
    float3 w = pow(an, sharpness); // an^sharpness
    float sum = w.x + w.y + w.z + 1e-5;
    w /= sum;

    return xTex * w.x + yTex * w.y + zTex * w.z;
}

float4 SampleFromSet(uint index, uint mappingType, float scale, float sharpness, float2 uv, float3 localPos, float3 normal)
{
    if (index == INVALID_TEXTURE_INDEX)
        return 1.0.xxxx;
    
    if (mappingType == ETextureMappingType::TRIPLANAR)
    {
        return SampleTriplanar(gMaterialTextures[index], gLinearSampler, localPos, normal, scale, sharpness);
    }
    else
    {
        // default UV mapping
        return gMaterialTextures[index].Sample(gLinearSampler, uv);
    }
}

#endif //TEXTURE_HLSLI