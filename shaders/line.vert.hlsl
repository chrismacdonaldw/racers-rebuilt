cbuffer ViewUniforms : register(b0, space1)
{
    float4x4 viewProjection;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

VertexOutput main(float3 position : TEXCOORD0, float4 color : TEXCOORD1)
{
    VertexOutput output;
    output.position = mul(viewProjection, float4(position, 1.0));
    output.color = color;
    return output;
}
