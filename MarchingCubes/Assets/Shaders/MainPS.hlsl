// MainPS.hlsl
#include "Common.hlsli"
#include "PBR.hlsli"
#include "Texture.hlsli"
#include "Material.hlsli"

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
    EvaluatedMaterial mat = EvaluateMaterial(gMaterials[gMaterialIndex], input.TexCoord, input.WorldPos, input.WorldNormal, input.WorldTangent, input.TangentSign);
    float3 V = normalize(gCameraPos - input.WorldPos);
    float3 N = mat.normal;
        
    // Default Reflection For Fresnel Function
    float3 F0;
    if (mat.shadingModel == EShadingModel::TRANSLUCENT)
    {
        // 금속, 유리 등의 환경 맵핑 처리
        float IOR = mat.IOR;
        float fd = pow((1.0f - IOR) / (1.0f + IOR), 2.0f);
        F0 = fd.xxx;
        
        float cosTheta = max(dot(N, V), 0.0f);
        float3 F = FresnelSchlick(cosTheta, F0);
        
        float3 R = reflect(-V, N);
        float3 T = refract(-V, N, 1.0f / IOR);
        
        float3 colRefl = gEnvMap.Sample(gLinearSampler, R).rgb;
        float3 colRefr = gEnvMap.Sample(gLinearSampler, T).rgb * mat.albedo;
        
        float3 LoEnv = colRefl * F + colRefr * (1.0f - F);
        
        float alpha = 1.0f - F.r;
        
        return float4(LoEnv + mat.emissive, alpha);
    }
    else if (mat.shadingModel == EShadingModel::DIELECTRIC)
    {
        // 유전체 모델
        F0 = ComputeF0_Dielectric(mat.IOR, mat.metalic, mat.albedo);
    }
    else //EShadingModel::DEFAULT_LIT
    {
        //기본 모델 : albedo x specularStrength 조합
        F0 = ComputeF0_Default(mat.albedo, mat.specularStrength, mat.metalic);
    }
    
    // 모든 라이트 순회
    float3 LoSum = float3(0, 0, 0);
    
    [loop]
    for (uint i = 0; i < g_NumLights; ++i)
    {
        Light light = g_Lights[i];
        
        float3 L;
        float attenuation = 1.0f;
        
        if (light.type == ELightType::DIRECTIONAL) // Directional Light
        {
            L = normalize(-light.param0);
        }
        else
        {
            float3 lightPos = light.param0;
            float3 posToLight = lightPos - input.WorldPos;
            float dist = length(posToLight);
            L = (dist > 0.0f) ? (posToLight / dist) : float3(0.0f, 0.0f, 0.0f);
            
            attenuation = saturate(1.f - dist / light.rangeOrPad);
            
            if (light.type == ELightType::SPOT) // Spot Light
            {
                float3 spotDir = normalize(light.spotParam1);
                float cosLA = dot(L, spotDir);
                
                // NOTE : outerCos은 상수로 정의.
                const float outerCos = 0.8f;
                float innerCos = light.spotParam2;
                
                float spotAtt = saturate((cosLA - outerCos) / (innerCos - outerCos));
                attenuation *= spotAtt;
            }
        }
    
        // BRDF
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0f);
        float NdotV = max(dot(N, V), 0.0f);
        
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
        float NDF = DistributionGGX(N, H, mat.roughness);
        float G = GeometrySmith(N, V, L, mat.roughness);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.001f;
        float3 specularBRDF = numerator / denominator;
        
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - mat.metalic);
        float3 diffuse = kD * mat.albedo / PI;
        float3 Lo = (diffuse + specularBRDF) * g_Lights[i].radiance * NdotL;
        
        LoSum += Lo;
    }
    
    // Ambient + Gamma
    float3 ambient = 0.03f * mat.albedo * mat.ambientOcclusion;
    float3 color = ambient + LoSum + mat.emissive;
    color = pow(color, 1.0f / 2.2f);
    
    return float4(color, mat.opacity);
}