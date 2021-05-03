cbuffer ResourceCB : register(b0)
{
    uint c_VertexBufferID;
    uint c_VertexOffset;
    uint c_TextureID;
    uint c_SamplerID;
};

cbuffer CB : register(b1)
{
    float4x4 ProjectionMatrix;
};

struct Vertex
{
    float2 pos;
    float2 uv;
    uint col;
};

#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24

float4 UnpackColor(uint color)
{
    float s = 1.0f / 255.0f;
    return float4(((color >> IM_COL32_R_SHIFT) & 0xFF) * s,
        ((color >> IM_COL32_G_SHIFT) & 0xFF) * s,
        ((color >> IM_COL32_B_SHIFT) & 0xFF) * s,
        ((color >> IM_COL32_A_SHIFT) & 0xFF) * s);
}

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};

PS_INPUT vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<Vertex> VB = ResourceDescriptorHeap[c_VertexBufferID];
    Vertex vertex = VB[vertex_id + c_VertexOffset];
    
    PS_INPUT output;
    output.pos = mul(ProjectionMatrix, float4(vertex.pos.xy, 0.f, 1.f));
    output.col = UnpackColor(vertex.col);
    output.uv = vertex.uv;
    return output;
}

float4 ps_main(PS_INPUT input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[c_TextureID];
    SamplerState linear_sampler = SamplerDescriptorHeap[c_SamplerID];
    float4 out_col = input.col * texture.Sample(linear_sampler, input.uv);
    out_col.xyz = pow(out_col.xyz, 2.2);
    return out_col;
}