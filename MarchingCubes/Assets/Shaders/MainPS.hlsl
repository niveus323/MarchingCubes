// MainPS.hlsl
#include "Common.hlsli"
#include "PBR.hlsli"

TextureCube gEnvMap : register(t0);
SamplerState gEnvSampler : register(s0);

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION0;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float4 Color : COLOR0;
};

float3 ComputeF0_Default(float3 albedo, float specular)
{
    float3 dielectricF0 = (0.08f * specular).xxx;
    return lerp(dielectricF0, albedo, gMetalic);
}

float3 ComputeF0_Dielectric(float ior, float metallic, float3 albedo)
{
    float fd = pow((1.0f - ior) / (1.0f + ior), 2.0f);
    float3 F0 = lerp(fd.xxx, albedo, metallic);
    
    return F0;
}


// Pixel Shader: output interpolated color
float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.WorldNormal);
    float3 V = normalize(gCameraPos - input.WorldPos);
    
    // Default Reflection For Fresnel Function
    float3 F0;
    if (gShadingModel == 0)
    {
        //기본 모델 : albedo x specularStrength 조합
        F0 = ComputeF0_Default(gAlbedo, gSpecularStrength);
    }
    else if (gShadingModel == 1)
    {
        // 유전체 모델
        F0 = ComputeF0_Dielectric(gIOR, gMetalic, gAlbedo);
    }
    else
    {
        // 금속, 유리 등의 환경 맵핑 처리
        
        float fd = pow((1.0f - gIOR) / (1.0f + gIOR), 2.0f);
        F0 = fd.xxx;
        
        float cosTheta = max(dot(N, V), 0.0f);
        float3 F = FresnelSchlick(cosTheta, F0);
        
        float3 R = reflect(-V, N);
        float3 T = refract(-V, N, 1.0f / gIOR);
        
        float3 colRefl = gEnvMap.Sample(gEnvSampler, R).rgb;
        float3 colRefr = gEnvMap.Sample(gEnvSampler, T).rgb * gAlbedo;
        
        float3 LoEnv = colRefl * F + colRefr * (1.0f - F);
        
        float alpha = 1.0f - F.r;
        
        return float4(LoEnv, alpha);
        
    }
    
    // 모든 라이트 순회
    float3 LoSum = float3(0, 0, 0);
    
    [unroll]
    for (uint i = 0; i < g_NumLights; ++i)
    {
        Light light = g_Lights[i];
        
        float3 L;
        float attenuation = 1.0f;
        
        if (light.type == 0) // Directional Light
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
            
            if (light.type == 2) // Spot Light
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
        float NDF = DistributionGGX(N, H, gRoughness);
        float G = GeometrySmith(N, V, L, gRoughness);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.001f;
        float3 specularBRDF = numerator / denominator;
        
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - gMetalic);
        float3 diffuse = kD * gAlbedo / PI;
        float3 Lo = (diffuse + specularBRDF) * g_Lights[i].radiance * NdotL;
        
        LoSum += Lo;
    }
    
    // Ambient + Gamma
    float3 ambient = 0.03f * gAlbedo * gAmbientOcclusion;
    float3 color = ambient + LoSum;
    color = pow(color, 1.0f / 2.2f);
    
    return float4(color, 1.0f);
}