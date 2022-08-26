#include "../bilinear.hlsli"
#include "blur_common.hlsli"
#include "sh.hlsli"

cbuffer CB : register(b1)
{
    uint c_inputSHMipsTexture;
    uint c_linearDepthMipsTexture;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_accumulationCountTexture;
    uint c_outputSHTexture;
}

#define MAX_FRAME_NUM_WITH_HISTORY_FIX (10.0)

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    float2 uv = GetScreenUV(pos, SceneCB.rcpRenderSize);
    
    Texture2D<uint4> inputSHMipsTexture = ResourceDescriptorHeap[c_inputSHMipsTexture];
    Texture2D<float> linearDepthMipsTexture = ResourceDescriptorHeap[c_linearDepthMipsTexture];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    Texture2D<uint> accumulationCountTexture = ResourceDescriptorHeap[c_accumulationCountTexture];
    RWTexture2D<uint4> outputSHTexture = ResourceDescriptorHeap[c_outputSHTexture];
    
    float depth = depthTexture[pos];
    if(depth == 0.0)
    {
        return;
    }
    
    float normAccumulatedFrameNum = saturate(accumulationCountTexture[pos] / MAX_FRAME_NUM_WITH_HISTORY_FIX);
    if (normAccumulatedFrameNum == 1.0)
    {
        return;
    }
    
    float3 worldPos = GetWorldPosition(pos, depth);
    float3 N = DecodeNormal(normalTexture[pos].xyz);

    uint mipLevel = round(3.0 * (1.0 - normAccumulatedFrameNum));
    float2 mipSize = SceneCB.renderSize / (1u << (mipLevel + 1));
    Bilinear filter = GetBilinearFilter(uv, mipSize);

    float z00 = GetNdcDepth(linearDepthMipsTexture.Load(uint3(filter.origin + uint2(0, 0), mipLevel)));
    float z10 = GetNdcDepth(linearDepthMipsTexture.Load(uint3(filter.origin + uint2(1, 0), mipLevel)));
    float z01 = GetNdcDepth(linearDepthMipsTexture.Load(uint3(filter.origin + uint2(0, 1), mipLevel)));
    float z11 = GetNdcDepth(linearDepthMipsTexture.Load(uint3(filter.origin + uint2(1, 1), mipLevel)));

    float4 customWeights;
    customWeights.x = GetGeometryWeight(worldPos, N, GetWorldPosition(uv, z00), 1.0);
    customWeights.y = GetGeometryWeight(worldPos, N, GetWorldPosition(uv, z10), 1.0);
    customWeights.z = GetGeometryWeight(worldPos, N, GetWorldPosition(uv, z01), 1.0);
    customWeights.w = GetGeometryWeight(worldPos, N, GetWorldPosition(uv, z11), 1.0);

    SH sh00 = UnpackSH(inputSHMipsTexture.Load(uint3(filter.origin + uint2(0, 0), mipLevel)));
    SH sh10 = UnpackSH(inputSHMipsTexture.Load(uint3(filter.origin + uint2(1, 0), mipLevel)));
    SH sh01 = UnpackSH(inputSHMipsTexture.Load(uint3(filter.origin + uint2(0, 1), mipLevel)));
    SH sh11 = UnpackSH(inputSHMipsTexture.Load(uint3(filter.origin + uint2(1, 1), mipLevel)));
    
    float4 bilinearWeights = GetBilinearCustomWeights(filter, customWeights);
    SH blurry = ApplyBilinearCustomWeights(sh00, sh10, sh01, sh11, bilinearWeights);

    outputSHTexture[pos] = PackSH(blurry);
}