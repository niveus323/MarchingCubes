// --------------------------------------
// Material.hlsli
// - Material 정의 및 헬퍼 함수
// --------------------------------------
#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI
#include "Texture.hlsli"

static const uint INVALID_MATERIAL_INDEX = 0xFFFFFFFF;
static const float ALPHA_CUT_OFF = 0.05f;

struct EShadingModel
{
    static const uint DEFAULT_LIT = 0;
    static const uint DIELECTRIC = 1;
    static const uint TRANSLUCENT = 2;
    static const uint UNLIT = 3;
};

struct TriplanarParams
{
    float scale;
    float sharpness;
    uint2 _padding0;
};

struct TextureParams
{
    uint diffuseIndex;
    uint normalIndex;
    uint armIndex;
    uint displacementIndex;
    
    uint roughnessIndex;
    uint emissiveIndex;
    uint metalicIndex;
    uint mappingType; // 0 - Default UV, 1 - Triplanar, 2 - Spherical ...
    
    // Triplanar
    TriplanarParams triplanar;
};

struct MaterialBuffer
{
    float3 albedo; // default color
    float metalic; // [0,1]
    
    float specularStrength; // [0,1], default = 0.5
    float roughness; // [0,1]
    float ambientOcclusion; // [0,1]
    float IOR; // Dielectric 전용
    
    float opacity; // [0,1], Default - 1
    uint shadingModel; //0 - Default, 1 - Dielectric, 2 - Translucent    
    uint2 _padding0;
    
    TextureParams baseTextures;
};
StructuredBuffer<MaterialBuffer> gMaterials : register(t0);

struct EvaluatedMaterial
{
    float3 albedo;
    float metalic;
    
    float specularStrength;
    float roughness;
    float ambientOcclusion;
    float IOR;

    uint shadingModel;
    float opacity;

    float3 emissive;
    float3 normal; // 최종 노멀 (normal map 포함)
};

float4 SampleMaterialTexture(uint index, TextureParams tex, float2 uv, float3 localPos, float3 worldNormal)
{
    return SampleFromSet(index, tex.mappingType, tex.triplanar.scale, tex.triplanar.sharpness, uv, localPos, worldNormal);
}

float3 SampleNormal(uint index, TextureParams tex, float2 uv, float3 localPos, float3 worldNormal, float3 worldTangent, float tangentSign)
{
    if (index == INVALID_TEXTURE_INDEX)
        return normalize(worldNormal);
    
    float3 nTS = SampleFromSet(index, tex.mappingType, tex.triplanar.scale, tex.triplanar.sharpness, uv, localPos, worldNormal).rgb;

    // [0,1] -> [-1,1]
    nTS = nTS * 2.0f - 1.0f;
    nTS = normalize(nTS);

    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent);
    float3 B = normalize(cross(N, T)) * tangentSign;

    return normalize(T * nTS.x + B * nTS.y + N * nTS.z);
}

EvaluatedMaterial EvaluateMaterial(MaterialBuffer mat, float2 uv, float3 localPos, float3 worldNormal, float3 worldTan, float tangentSign)
{
    EvaluatedMaterial outMat;
    outMat.albedo = mat.albedo;
    outMat.metalic = mat.metalic;
    outMat.specularStrength = mat.specularStrength;
    outMat.roughness = mat.roughness;
    outMat.ambientOcclusion = mat.ambientOcclusion;
    outMat.IOR = mat.IOR;
    outMat.shadingModel = mat.shadingModel;
    outMat.opacity = mat.opacity;
    outMat.emissive = 0.0.xxx;

    TextureParams tex = mat.baseTextures;

    // Diffuse 텍스처
    if (tex.diffuseIndex != INVALID_TEXTURE_INDEX)
    {
        float4 texAlbedo = SampleMaterialTexture(tex.diffuseIndex, tex, uv, localPos, worldNormal);
        outMat.albedo *= texAlbedo.rgb;
        outMat.opacity *= texAlbedo.a;
    }

    // ARM 텍스쳐
    if (tex.armIndex != INVALID_TEXTURE_INDEX)
    {
        float3 arm = SampleMaterialTexture(tex.armIndex, tex, uv, localPos, worldNormal).rgb;
        outMat.ambientOcclusion *= arm.r;
        outMat.roughness *= arm.g;
        outMat.metalic *= arm.b;
    }
    else 
    {
        // Roughness 텍스쳐
        if (tex.roughnessIndex != INVALID_TEXTURE_INDEX)
        {
            float roughTex = SampleMaterialTexture(tex.roughnessIndex, tex, uv, localPos, worldNormal).r;
            outMat.roughness *= roughTex;
        }
    }

    // Emissive
    if (tex.emissiveIndex != INVALID_TEXTURE_INDEX)
    {
        outMat.emissive = SampleMaterialTexture(tex.emissiveIndex, tex, uv, localPos, worldNormal).rgb;
    }

    // TODO : Displacement, Metalic 텍스쳐 적용
    
    // Normal (normal map 포함 최종 노멀)
    outMat.normal = SampleNormal(tex.normalIndex, tex, uv, localPos, worldNormal, worldTan, tangentSign);

    return outMat;
}
#endif // MATERIAL_HLSLI