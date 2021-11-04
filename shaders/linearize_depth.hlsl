#include "common.hlsli"

cbuffer CB : register(b0)
{
    uint c_depthRT;
    uint c_linearDepthRT;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pos = dispatchThreadID.xy;
    
    Texture2D depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT[pos].x;
    
    float linearDepth = GetLinearDepth(depth);
    
    RWTexture2D<float> linearDepthTexture = ResourceDescriptorHeap[c_linearDepthRT];
    linearDepthTexture[pos] = linearDepth;
}