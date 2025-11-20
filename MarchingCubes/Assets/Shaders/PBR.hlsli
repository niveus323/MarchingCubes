// ---------------------------------------------
// PBR.hlsli
// - Cook-Torrance based PBR 유틸 함수
// ---------------------------------------------
#ifndef PBR_HLSLI
#define PBR_HLSLI

static const float PI = 3.14159265359f;

// Trowbridge-Reitz (GGX) Normal Distribution Function
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    return a2 / denom;
}

// Geometry Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

// Smith’s method
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float ggx1 = GeometrySchlickGGX(max(dot(N, V), 0.0f), roughness);
    float ggx2 = GeometrySchlickGGX(max(dot(N, L), 0.0f), roughness);
    return ggx1 * ggx2;
}

// Fresnel Schlick 근사
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float3 ComputeF0_Default(float3 albedo, float specular, float metalic)
{
    float3 dielectricF0 = (0.08f * specular).xxx;
    return lerp(dielectricF0, albedo, metalic);
}

float3 ComputeF0_Dielectric(float ior, float metallic, float3 albedo)
{
    float fd = pow((1.0f - ior) / (1.0f + ior), 2.0f);
    float3 F0 = lerp(fd.xxx, albedo, metallic);
    
    return F0;
}

#endif // PBR_HLSLI