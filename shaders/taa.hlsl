cbuffer CB : register(b1)
{
    uint c_inputTexture;
    uint c_historyRT;
    uint c_outTexture;
    uint c_width;
    uint c_height;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_width || dispatchThreadID.y >= c_height)
    {
        return;
    }
    
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    Texture2D historyTexture = ResourceDescriptorHeap[c_historyRT];
    
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outTexture];
    
    outputTexture[dispatchThreadID.xy] = lerp(inputTexture[dispatchThreadID.xy], historyTexture[dispatchThreadID.xy], 0.95);
}