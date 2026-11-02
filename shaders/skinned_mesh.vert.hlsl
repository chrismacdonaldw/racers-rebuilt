cbuffer DrawUniforms : register(b0, space1)
{
    float4x4 model;
    float4x4 viewProjection;
};

cbuffer JointUniforms : register(b1, space1)
{
    float4x4 joints[40];
};

struct VertexInput
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 color : TEXCOORD3;
    uint2 joint : TEXCOORD4;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float4x4 placed = mul(model, joints[min(input.joint.x, 39u)]);
    output.position = mul(viewProjection, mul(placed, float4(input.position, 1.0)));
    output.normal = mul((float3x3)placed, input.normal);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
