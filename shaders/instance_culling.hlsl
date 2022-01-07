
cbuffer RootConstants : register(b0)
{
    uint c_dispatchCount;
    uint c_counterBufferSRV;
    uint c_commandBufferUAV;
};

[numthreads(64, 1, 1)]
void build_2nd_phase_indirect_command(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint dispatchIndex = dispatchThreadID.x;
    if (dispatchIndex >= c_dispatchCount)
    {
        return;
    }

    ByteAddressBuffer counterBuffer = ResourceDescriptorHeap[c_counterBufferSRV];
    RWStructuredBuffer<uint3> commandBuffer = ResourceDescriptorHeap[c_commandBufferUAV];

    uint cullmeshletsCount = counterBuffer.Load(dispatchIndex * 4);
    commandBuffer[dispatchIndex] = uint3((cullmeshletsCount + 31) / 32, 1, 1);

}