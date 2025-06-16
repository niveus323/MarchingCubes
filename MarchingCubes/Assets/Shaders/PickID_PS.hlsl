float4 idColor : register(b2);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 Color : COLOR; // 개발 편의를 위해 Input을 통일. 해당 Hlsl에서는 사용되지 않음.
};

float4 PSMain(PSInput input) : SV_TARGET
{
  //  return idColor;
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}