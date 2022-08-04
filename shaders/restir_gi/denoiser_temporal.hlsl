
cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_historyTexture;
    uint c_outputTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    Texture2D historyTexture = ResourceDescriptorHeap[c_historyTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    uint2 pos = dispatchThreadID.xy;
    float3 inputRadiance = inputTexture[pos].xyz;
    float3 historyRadiance = historyTexture[pos].xyz;
    
    outputTexture[pos] = float4(lerp(inputRadiance, historyRadiance, 0.95), 1.0);
}