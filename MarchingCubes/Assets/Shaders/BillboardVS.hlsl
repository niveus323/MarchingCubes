// --------------------------------------
// BillboardVS.hlsl
// - Billboard가 사용할 Vertex Shader
// --------------------------------------
#include "Common.hlsli"

struct VSInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};
struct PSInput
{
    float4 Position : SV_POSITION0;
    float2 TexCoord : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    // Billboard는 카메라를 바라보도록 설정할 필요가 있다.
    float3 worldPos = gWorld[3].xyz; // Billboard의 중심 월드 위치는 Transform의 이동 변환 xyz 값
    float scaleX = length(gWorld[0].xyz);
    float scaleY = length(gWorld[1].xyz);
    float3 forward = normalize(gCameraPos - worldPos);
    float3 worldUp = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(forward, worldUp)) > 0.999f)
    {
        worldUp = float3(0.0f, 0.0f, 1.0f);
    }
    
    float3 right = normalize(cross(worldUp, forward));
    float3 up = cross(forward, right);
    float3 vertexPos = worldPos +
                        (right * input.Position.x * scaleX) +  // 카메라 Right 벡터 기준으로 Left, Right 설정
                        (up * input.Position.y * scaleY);      // 카메라 Up 벡터 기준으로 Top, Bottom 설정
    output.Position = mul(float4(vertexPos, 1), gViewProj);
    output.TexCoord = input.TexCoord;
    
    //NOTE : Game용 Billboard 구현 시 라이팅 계산을 위해 Output 구조체를 수정하고 아래 주석을 해제.
    //output.WorldPos = worldPos.xyz; 
    //float3x3 worldInvT = transpose((float3x3) gWorldInv);
    //output.WorldNormal = normalize(mul(input.Normal, worldInvT));
    //output.WorldTangent = normalize(mul(input.Tangent.xyz, worldInvT));
    //output.TangentSign = input.Tangent.w;
    //output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    return output;
}