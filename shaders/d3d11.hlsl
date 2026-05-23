struct vs_input
{
    float2 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 custom_data : TEXCOORD1;
    float4 custom_data2 : TEXCOORD2;
};

struct ps_input
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 custom_data : TEXCOORD1;
    float4 custom_data2 : TEXCOORD2;
};

Texture2D textr : register(t0);
SamplerState samplr : register(s0);

ps_input vertex_shader(vs_input input)
{
    ps_input output;

    output.position = float4(input.position, 0.f, 1.f);
    output.color = input.color;
    output.uv = input.uv;
    output.custom_data = input.custom_data;
    output.custom_data2 = input.custom_data2;
    
    return output;
}

float4 pixel_shader(ps_input input) : SV_TARGET
{
    return input.color * textr.Sample(samplr, input.uv);
}

float sd_round_rect(float2 p, float2 b, float4 r)
{
    float max_rad = min(b.x, b.y);
    r = min(r, max_rad);

    float2 s = step(0.0, p);
    
    float rad_top = lerp(r.w, r.x, s.x);
    float rad_bottom = lerp(r.z, r.y, s.x);
    float rad = lerp(rad_top, rad_bottom, s.y);

    float2 q = abs(p) - b + rad;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rad;
}

float4 rect_pixel_shader(ps_input input) : SV_TARGET
{
    float2 rect_size = input.custom_data.xy;
    float thickness = input.custom_data.z;
    float4 radii = input.custom_data2;
    
    float2 p = input.uv;
    float d = sd_round_rect(p, rect_size * 0.5f, radii);
    
    if (thickness > 0.0)
    {
        d = abs(d + thickness * 0.5f) - thickness * 0.5f;
    }
    
    float alpha = saturate(0.5f - d);
    
    return float4(input.color.rgb, input.color.a * alpha);
}

float4 shadow_pixel_shader(ps_input input) : SV_TARGET
{
    float2 rect_size = input.custom_data.xy;
    float cutout = input.custom_data.z;
    float shadow_blur = max(input.custom_data.w, 0.001f);
    float4 radii = input.custom_data2;
    
    float2 p = input.uv;
    float d = sd_round_rect(p, rect_size * 0.5f, radii);
    
    float cutout_alpha = 1.0f;
    if (cutout > 0.5f)
    {
        cutout_alpha = saturate(d + 0.5f);
    }
    
    float dist = max(d, 0.0f);
    float x = saturate(dist / shadow_blur);
    
    float alpha = pow(1.0f - x, 3.0f) * cutout_alpha;
    
    return float4(input.color.rgb, input.color.a * alpha);
}

float4 image_pixel_shader(ps_input input) : SV_TARGET
{
    float2 rect_size = input.custom_data.xy;
    float4 radii = input.custom_data2;

    float2 p = (input.uv - 0.5f) * rect_size;
    
    float d = sd_round_rect(p, rect_size * 0.5f, radii);
    
    float alpha = saturate(0.5f - d);
    
    float4 tex_color = textr.Sample(samplr, input.uv);
    return float4(tex_color.rgb * input.color.rgb, tex_color.a * input.color.a * alpha);
}