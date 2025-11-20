// MainPS.hlsl
#include "Common.hlsli"
#include "PBR.hlsli"
#include "Texture.hlsli"

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION0;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float4 Color : COLOR0;
};

float3 SampleMaterialTexture(float2 uv, float3 worldPos, float3 normal, TextureParams param)
{
    uint texIndex = param.texIndex;
    uint mappingType = param.mappingType;
    if (texIndex == INVALID_TEXTURE_INDEX)
        return float3(1.0, 1.0, 1.0);
    
    if (mappingType == ETextureMappingType::TRIPLANAR)
    {
        return SampleTriplanar(gMaterialTextures[texIndex], gLinearSampler, worldPos, normal, param.triplanar.scale, param.triplanar.sharpness);
    }
    else
    {
        // default UV mapping
        return gMaterialTextures[texIndex].Sample(gLinearSampler, uv).rgb;
    }
    
    return float3(1.0, 1.0, 1.0);
}

// Pixel Shader: output interpolated color
float4 PSMain(PSInput input) : SV_TARGET
{
    MaterialBuffer mat = gMaterials[gMaterialIndex];
    float3 albedo = mat.albedo * SampleMaterialTexture(input.TexCoord, input.WorldPos, input.WorldNormal, mat.diffuse);
    float metalic = mat.metalic;
    float specularStrength = mat.specularStrength;
    float roughness = mat.roughness;
    float ambientOcclusion = mat.ambientOcclusion;
    float IOR = mat.IOR;
    
    uint shadingModel = mat.shadingModel;
    float opacity = mat.opacity;
    
    float3 N = normalize(input.WorldNormal);
    float3 V = normalize(gCameraPos - input.WorldPos);
    
    // Default Reflection For Fresnel Function
    float3 F0;
    if (shadingModel == EShadingModel::TRANSLUCENT)
    {
        // 금속, 유리 등의 환경 맵핑 처리
        
        float fd = pow((1.0f - IOR) / (1.0f + IOR), 2.0f);
        F0 = fd.xxx;
        
        float cosTheta = max(dot(N, V), 0.0f);
        float3 F = FresnelSchlick(cosTheta, F0);
        
        float3 R = reflect(-V, N);
        float3 T = refract(-V, N, 1.0f / IOR);
        
        float3 colRefl = gEnvMap.Sample(gLinearSampler, R).rgb;
        float3 colRefr = gEnvMap.Sample(gLinearSampler, T).rgb * albedo;
        
        float3 LoEnv = colRefl * F + colRefr * (1.0f - F);
        
        float alpha = 1.0f - F.r;
        
        return float4(LoEnv, alpha);
    }
    else if (shadingModel == EShadingModel::DIELECTRIC)
    {
        // 유전체 모델
        F0 = ComputeF0_Dielectric(IOR, metalic, albedo);
    }
    else //EShadingModel::DEFAULT_LIT
    {
        //기본 모델 : albedo x specularStrength 조합
        F0 = ComputeF0_Default(albedo, specularStrength, metalic);
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
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.001f;
        float3 specularBRDF = numerator / denominator;
        
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metalic);
        float3 diffuse = kD * albedo / PI;
        float3 Lo = (diffuse + specularBRDF) * g_Lights[i].radiance * NdotL;
        
        LoSum += Lo;
    }
    
    // Ambient + Gamma
    float3 ambient = 0.03f * albedo * ambientOcclusion;
    float3 color = ambient + LoSum;
    color = pow(color, 1.0f / 2.2f);
    
    return float4(color, 1.0f);
}