// HighlightPS.hlsl

// Camera constant buffer: register b0
cbuffer CameraBuffer : register(b0)
{
    matrix gViewProj; // View-Projection matrix
};

cbuffer ObjectBuffer : register(b1)
{
    matrix gWorld;
};

// Vertex input structure
struct VSInput
{
    float3 Position : POSITION;
};

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION;
};

// Vertex Shader: transform position by view-projection
PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 modelPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(modelPos, gWorld);
    output.Position = mul(worldPos, gViewProj);
    return output;
}

// Pixel Shader: output interpolated color
float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
