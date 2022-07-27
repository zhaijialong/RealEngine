
cbuffer CB : register(b0)
{
    uint c_reservoirTexture;
    uint c_irradianceTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D irradianceTexture = ResourceDescriptorHeap[c_irradianceTexture];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_reservoirTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    
    float3 irradiance = irradianceTexture[pos].xyz;
    float W = reservoirTexture[pos].y;
    outputTexture[pos] = float4(irradiance * W, 0.0);
}