Texture2D<float4> baseTexture : register(t0, space2);
SamplerState baseSampler : register(s0, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    float4 tint;
    float4 ambient;
    float3 lightDirection;
    float alphaCutoff;
    float modulateWeight;
};

static const float kMinNormalLengthSquared = 1e-6;

float3 Shade(float3 normal, float4 vertexColor)
{
    float lengthSquared = dot(normal, normal);
    if (lengthSquared < kMinNormalLengthSquared)
    {
        return vertexColor.rgb;
    }
    float diffuse = saturate(dot(normal * rsqrt(lengthSquared), -lightDirection));
    return saturate(ambient.rgb + (tint.rgb * diffuse));
}

float4 main(float4 position : SV_Position, float3 normal : TEXCOORD0, float2 uv : TEXCOORD1,
            float4 vertexColor : TEXCOORD2) : SV_Target0
{
    float4 texel = baseTexture.Sample(baseSampler, uv);
    float alpha = texel.a * tint.a;
    clip(alpha - alphaCutoff);
    return float4(texel.rgb * lerp(1.0, Shade(normal, vertexColor), modulateWeight), alpha);
}
