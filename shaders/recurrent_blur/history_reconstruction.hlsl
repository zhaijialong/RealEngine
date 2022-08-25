
cbuffer CB : register(b0)
{
    uint c_inputSHMipsTexture;
    uint c_accumulationCountTexture;
    uint c_outputSHTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputSHMipsTexture = ResourceDescriptorHeap[c_inputSHMipsTexture];
    Texture2D<uint> accumulationCountTexture = ResourceDescriptorHeap[c_accumulationCountTexture];
    RWTexture2D<uint4> outputSHTexture = ResourceDescriptorHeap[c_outputSHTexture];
    
    if (accumulationCountTexture[pos] < 20)
    {
        outputSHTexture[pos] = uint4(11, 30235, 23509, 2304943);
    }
}