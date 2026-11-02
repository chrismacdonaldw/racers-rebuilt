Texture2D<float4> baseTexture : register(t0, space2);
SamplerState baseSampler : register(s0, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    float4 tint;
    float4 ambient;
    float alphaCutoff;
    float modulateWeight;
    float2 uvOffset;
    float4 vertexScale;
    float4 vertexOffset;
};

static const uint kMaxLights = 7;

cbuffer LightUniforms : register(b1, space3)
{
    float4 lightDirections[kMaxLights];
    float4 lightColors[kMaxLights];
    uint lightCount;
};

static const float kMinNormalLengthSquared = 1e-6;

float3 Shade(float3 normal, float4 vertexColor)
{
    float lengthSquared = dot(normal, normal);
    if (lengthSquared < kMinNormalLengthSquared)
    {
        return min(1.0, (vertexColor.rgb * vertexScale.rgb) + vertexOffset.rgb);
    }
    float3 unit = normal * rsqrt(lengthSquared);
    float3 lit = ambient.rgb;
    for (uint i = 0; i < lightCount; ++i)
    {
        float diffuse = saturate(dot(unit, -lightDirections[i].xyz));
        lit += tint.rgb * lightColors[i].rgb * diffuse;
    }
    return saturate(lit);
}

float4 main(float4 position : SV_Position, float3 normal : TEXCOORD0, float2 uv : TEXCOORD1,
            float4 vertexColor : TEXCOORD2) : SV_Target0
{
    float4 texel = baseTexture.Sample(baseSampler, uv + uvOffset);
    float alpha = texel.a * tint.a;
    clip(alpha - alphaCutoff);
    return float4(texel.rgb * lerp(1.0, Shade(normal, vertexColor), modulateWeight), alpha);
}
