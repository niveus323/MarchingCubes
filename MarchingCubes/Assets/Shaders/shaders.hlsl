// BasicCameraShaders.hlsl

// Camera constant buffer: register b0
cbuffer CameraBuffer : register(b0)
{
    matrix gViewProj; // View-Projection matrix
};

cbuffer ObjectBuffer : register(b1)
{
    matrix gWorld;
    float4 gObjectColor;
};

// Vertex input structure
struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

// Vertex-to-pixel output structure
struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

// Vertex Shader: transform position by view-projection
PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 modelPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(modelPos, gWorld);
    output.Position = mul(worldPos, gViewProj);
    output.Color = input.Color;
    return output;
}

// Pixel Shader: output interpolated color
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.Color * gObjectColor;
}
