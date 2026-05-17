struct vs_input
{
    float2 position : POSITION;
    float4 color : COLOR;
};

struct ps_input
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

ps_input vertex_shader(vs_input input)
{
    ps_input output;

    output.position = float4(input.position, 0.f, 1.f);
    output.color = input.color;

    return output;
}

float4 pixel_shader(ps_input input) : SV_TARGET
{
    return input.color;
}