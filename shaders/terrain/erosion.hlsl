#include "noise.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_passIndex;
}

cbuffer CB1 : register(b1)
{
    uint c_heightmapUAV;
    uint c_hardnessUAV;
    uint c_seidmentUAV;
    uint c_waterUAV;
    uint c_fluxUAV;
    uint c_velocityUAV;
}

static RWTexture2D<float> heightmapUAV = ResourceDescriptorHeap[c_heightmapUAV];
static RWTexture2D<float> hardnessUAV = ResourceDescriptorHeap[c_hardnessUAV];
static RWTexture2D<float> sedimentUAV = ResourceDescriptorHeap[c_seidmentUAV];
static RWTexture2D<float> waterUAV = ResourceDescriptorHeap[c_waterUAV];
static RWTexture2D<float4> fluxUAV = ResourceDescriptorHeap[c_fluxUAV];
static RWTexture2D<float2> velocityUAV = ResourceDescriptorHeap[c_velocityUAV];

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    
}