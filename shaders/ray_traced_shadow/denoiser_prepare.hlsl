#include "../common.hlsli"

cbuffer CB : register(b0)
{
    uint c_raytracedResult;
    uint c_shadowMaskBuffer;
}

int2 FFX_DNSR_Shadows_GetBufferDimensions()
{
    return int2(SceneCB.viewWidth, SceneCB.viewHeight);
}

bool FFX_DNSR_Shadows_HitsLight(uint2 did, uint2 gtid, uint2 gid)
{
    Texture2D raytracedResult = ResourceDescriptorHeap[c_raytracedResult];
    return raytracedResult[did].x == 1;
}

void FFX_DNSR_Shadows_WriteMask(uint offset, uint value)
{
    RWStructuredBuffer<uint> shadowMaskBuffer = ResourceDescriptorHeap[c_shadowMaskBuffer];
    shadowMaskBuffer[offset] = value;
}

#include "ffx-shadows-dnsr/ffx_denoiser_shadows_prepare.h"

[numthreads(8, 4, 1)]
void main(uint2 gtid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    FFX_DNSR_Shadows_PrepareShadowMask(gtid, gid);
}