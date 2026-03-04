// --------------------------------------
// EditorBillboardPS.hlsl
// - Billboard가 사용할 Pixel Shader
// --------------------------------------
#include "Common.hlsli"
#include "Texture.hlsli"
#include "Material.hlsli"

struct PSInput
{
    float4 Position : SV_POSITION0;
    float2 TexCoord : TEXCOORD2;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    // Editor용 Billboard는 라이팅 계산을 하지 않는다.
    // 화면에 출력할 아이콘 텍스쳐(diffuseTexture)와 색상 (albedo)값만 계산에 사용.
    
    if(gMaterialIndex == INVALID_MATERIAL_INDEX)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    MaterialBuffer mat = gMaterials[gMaterialIndex];
    
    float3 color = mat.albedo;
    float alpha = mat.opacity;
    uint textureIndex = mat.baseTextures.diffuseIndex;
    if(textureIndex != INVALID_TEXTURE_INDEX)
    {
        // UV Mapping을 사용하므로 position, normal은 MaterialTexture 계산에 사용되지 않음.
        float4 texColor = gMaterialTextures[textureIndex].Sample(gLinearSampler, input.TexCoord);
        color *= texColor.rgb;
        alpha *= texColor.a;
    }
    
    clip(alpha - 0.1f);
    
    return float4(color, alpha);
}