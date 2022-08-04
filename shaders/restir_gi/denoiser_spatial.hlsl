cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint c_outputHistoryTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    RWTexture2D<float4> outputhistoryTexture = ResourceDescriptorHeap[c_outputHistoryTexture];
    
    uint2 pos = dispatchThreadID.xy;
    float3 inputRadiance = inputTexture[pos].xyz;
    
    outputTexture[pos] = float4(inputRadiance, 1.0);
    outputhistoryTexture[pos] = float4(inputRadiance, 1.0);
}