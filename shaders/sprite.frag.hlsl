Texture2D<float4> spriteTexture : register(t0, space2);
SamplerState spriteSampler : register(s0, space2);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0, float4 color : TEXCOORD1)
    : SV_Target0
{
    return spriteTexture.Sample(spriteSampler, uv) * color;
}
