struct vs_input
{
    float2 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct ps_input
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

Texture2D textr : register(t0);
SamplerState samplr : register(s0);

ps_input vertex_shader(vs_input input)
{
    ps_input output;

    output.position = float4(input.position, 0.f, 1.f);
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}

float4 pixel_shader(ps_input input) : SV_TARGET
{
    return input.color * textr.Sample(samplr, input.uv);
}