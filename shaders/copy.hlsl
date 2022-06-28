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

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_depthTexture;
    uint c_pointSampler;
    uint padding0;
    uint padding1;
};

float4 ps_main(VSOutput input
#if OUTPUT_DEPTH
    , out float outDepth : SV_Depth
#endif
    ) : SV_TARGET
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[c_pointSampler];
    
#if OUTPUT_DEPTH
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    outDepth = depthTexture.SampleLevel(pointSampler, input.uv, 0);
#endif

    return inputTexture.SampleLevel(pointSampler, input.uv, 0);
}
