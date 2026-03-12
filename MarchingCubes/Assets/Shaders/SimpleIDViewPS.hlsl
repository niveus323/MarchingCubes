// SimpleIDViewPS.hlsl
cbuffer IdBuffer : register(b3)
{
    uint gID;
};

struct PSInput
{
    float4 Position : SV_POSITION0;
};

float4 main(PSInput input) : SV_TARGET
{
    if (gID == 0) return float4(0.0f, 0.0f, 0.0f, 1.0f);

    uint r = 0;
    uint g = 0;
    uint b = 0;
    uint temp = gID;

    // Bit Interleaving 방식으로 RGB 채널을 분배 (상위 비트일수록 낮은 값이 채널에 분배된다)
    [unroll]
    for (int i = 7; i >= 0; --i)
    {
        r |= (temp & 1) << i;
        temp >>= 1;
        g |= (temp & 1) << i;
        temp >>= 1;
        b |= (temp & 1) << i;
        temp >>= 1;
    }
    
    return float4((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f, 1.0f);
}