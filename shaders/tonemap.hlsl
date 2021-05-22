cbuffer ResourceCB : register(b0)
{
    uint c_hdrTexture;
    uint c_pointSampler;
    uint c_padding0;
    uint c_padding1;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    output.pos.x = (float) (vertex_id / 2) * 4.0 - 1.0;
    output.pos.y = (float) (vertex_id % 2) * 4.0 - 1.0;
    output.pos.z = 0.0;
    output.pos.w = 1.0;
    output.uv.x = (float) (vertex_id / 2) * 2.0;
    output.uv.y = 1.0 - (float) (vertex_id % 2) * 2.0;

    return output;
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[c_pointSampler];
    
    float3 color = hdrTexture.Sample(pointSampler, input.uv).xyz;
    color = ACESFilm(color);
    
    return float4(color, 1.0);
}