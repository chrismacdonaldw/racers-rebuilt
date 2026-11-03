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
    float flatWeight;
};

static const uint kMaxLights = 7;

cbuffer LightUniforms : register(b1, space3)
{
    float4 lightDirections[kMaxLights];
    float4 lightColors[kMaxLights];
    uint lightCount;
};

static const float kMinNormalLengthSquared = 1e-6;

bool IsPrelit(float3 normal)
{
    return dot(normal, normal) < kMinNormalLengthSquared;
}

float3 Shade(float3 normal, float4 vertexColor)
{
    if (IsPrelit(normal))
    {
        return min(1.0, (vertexColor.rgb * vertexScale.rgb) + vertexOffset.rgb);
    }
    float3 unit = normal * rsqrt(dot(normal, normal));
    float3 lit = ambient.rgb;
    for (uint i = 0; i < lightCount; ++i)
    {
        float diffuse = saturate(dot(unit, -lightDirections[i].xyz));
        lit += tint.rgb * lightColors[i].rgb * diffuse;
    }
    return saturate(lit);
}

struct PixelInput
{
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    nointerpolation float3 flatNormal : TEXCOORD3;
    nointerpolation float4 flatColor : TEXCOORD4;
};

float4 main(PixelInput input) : SV_Target0
{
    float3 normal = lerp(input.normal, input.flatNormal, flatWeight);
    float4 vertexColor = lerp(input.color, input.flatColor, flatWeight);
    float4 texel = baseTexture.Sample(baseSampler, input.uv + uvOffset);
    float alpha = texel.a * tint.a;
    clip(alpha - alphaCutoff);
    float vertexAlpha = IsPrelit(normal) ? vertexColor.a : 1.0;
    return float4(texel.rgb * lerp(1.0, Shade(normal, vertexColor), modulateWeight),
                  alpha * lerp(1.0, vertexAlpha, modulateWeight));
}
