#include "common.hlsli"

cbuffer spdConstants : register(b1)
{
    uint c_mips;
    uint c_numWorkGroups;
    uint2 c_workGroupOffset;
    
    float2 c_invInputSize;
    uint c_imgSrc;
    uint c_spdGlobalAtomicUAV;

    //hlsl packing rules : every element in an array is stored in a four-component vector
    uint4 c_imgDst[12]; // do no access MIP [5]
}

#define A_GPU
#define A_HLSL

#include "ffx_a.h"

groupshared AU1 spdCounter;
groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];

AF4 SpdLoadSourceImage(ASU2 p, AU1 slice)
{
    Texture2D imgSrc = ResourceDescriptorHeap[c_imgSrc];
    SamplerState minSampler = SamplerDescriptorHeap[SceneCB.minReductionSampler];
    SamplerState maxSampler = SamplerDescriptorHeap[SceneCB.maxReductionSampler];
    AF2 textureCoord = p * c_invInputSize + c_invInputSize;

#if MIN_MAX_FILTER
    AF4 result = AF4(imgSrc.SampleLevel(minSampler, textureCoord, 0).x, imgSrc.SampleLevel(maxSampler, textureCoord, 0).y, 0, 0);
#else
    AF4 result = AF4(imgSrc.SampleLevel(minSampler, textureCoord, 0).x, 0, 0, 0);
#endif

    return result;
}
AF4 SpdLoad(ASU2 tex, AU1 slice)
{
#if MIN_MAX_FILTER
    globallycoherent RWTexture2D<float2> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
    return float4(imgDst5[tex], 0, 0);
#else
    globallycoherent RWTexture2D<float> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
    return float4(imgDst5[tex], 0, 0, 0);
#endif
}
void SpdStore(ASU2 pix, AF4 outValue, AU1 index, AU1 slice)
{
    if (index == 5)
    {
#if MIN_MAX_FILTER
        globallycoherent RWTexture2D<float2> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
        imgDst5[pix] = outValue.xy;
#else
        globallycoherent RWTexture2D<float> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
        imgDst5[pix] = outValue.x;
#endif
        return;
    }
    
#if MIN_MAX_FILTER
    RWTexture2D<float2> imgDst = ResourceDescriptorHeap[c_imgDst[index].x];
    imgDst[pix] = outValue.xy;
#else
    RWTexture2D<float> imgDst = ResourceDescriptorHeap[c_imgDst[index].x];
    imgDst[pix] = outValue.x;
#endif
}
void SpdIncreaseAtomicCounter(AU1 slice)
{
    globallycoherent RWBuffer<uint> spdGlobalAtomic = ResourceDescriptorHeap[c_spdGlobalAtomicUAV];
    InterlockedAdd(spdGlobalAtomic[0], 1, spdCounter);
}
AU1 SpdGetAtomicCounter()
{
    return spdCounter;
}
void SpdResetAtomicCounter(AU1 slice)
{
    globallycoherent RWBuffer<uint> spdGlobalAtomic = ResourceDescriptorHeap[c_spdGlobalAtomicUAV];
    spdGlobalAtomic[0] = 0;
}
AF4 SpdLoadIntermediate(AU1 x, AU1 y)
{
#if MIN_MAX_FILTER
    return AF4(spdIntermediateR[x][y], spdIntermediateG[x][y], 0, 0);
#else
    return AF4(spdIntermediateR[x][y], 0, 0, 0);
#endif
}
void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
    spdIntermediateR[x][y] = value.x;
#if MIN_MAX_FILTER
    spdIntermediateG[x][y] = value.y;
#endif
}
AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
#if MIN_MAX_FILTER
    return float4(min(min(v0.x, v1.x), min(v2.x, v3.x)), max(max(v0.y, v1.y), max(v2.y, v3.y)), 0, 0);
#else
    return float4(min(min(v0.x, v1.x), min(v2.x, v3.x)), 0, 0, 0);
#endif
}

#define SPD_LINEAR_SAMPLER //we'are using a min reduction sampler

#include "ffx_spd.h"

[numthreads(256, 1, 1)]
void build_hzb(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
    SpdDownsample(
        AU2(WorkGroupId.xy), 
        AU1(LocalThreadIndex),  
        AU1(c_mips),
        AU1(c_numWorkGroups),
        AU1(WorkGroupId.z),
        AU2(c_workGroupOffset));
 }
