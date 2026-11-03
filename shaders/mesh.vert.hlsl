cbuffer DrawUniforms : register(b0, space1)
{
    float4x4 model;
    float4x4 viewProjection;
};

struct VertexInput
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 color : TEXCOORD3;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    nointerpolation float3 flatNormal : TEXCOORD3;
    nointerpolation float4 flatColor : TEXCOORD4;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(viewProjection, mul(model, float4(input.position, 1.0)));
    output.normal = mul((float3x3)model, input.normal);
    output.uv = input.uv;
    output.color = input.color;
    output.flatNormal = output.normal;
    output.flatColor = input.color;
    return output;
}
