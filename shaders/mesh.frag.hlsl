Texture2D<float4> baseTexture : register(t0, space2);
SamplerState baseSampler : register(s0, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    float4 tint;
    float3 lightDirection;
    float alphaCutoff;
};

static const float kAmbient = 0.35;

float4 main(float4 position : SV_Position, float3 normal : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0
{
    float4 color = baseTexture.Sample(baseSampler, uv) * tint;
    clip(color.a - alphaCutoff);
    float diffuse = saturate(dot(normalize(normal), -lightDirection));
    return float4(color.rgb * lerp(kAmbient, 1.0, diffuse), color.a);
}
