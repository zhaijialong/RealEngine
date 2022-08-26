#include "../common.hlsli"
#include "sh.hlsli"

cbuffer spdConstants : register(b1)
{
    uint c_mips;
    uint c_numWorkGroups;
    uint2 c_workGroupOffset;
    
    float2 c_invInputSize;
    uint c_inputSHTexture;
    uint c_spdGlobalAtomicUAV;

    //hlsl packing rules : every element in an array is stored in a four-component vector
    uint4 c_outputSHTexture[12]; //only 4 mips
}

#define A_GPU
#define A_HLSL
#include "../ffx_a.h"

groupshared AU1 spdCounter;
groupshared float spdIntermediate[16][16];

AF4 SpdLoadSourceImage(ASU2 p, AU1 slice)
{
    Texture2D<float> imgSrc = ResourceDescriptorHeap[c_inputSHTexture];
    return AF4(imgSrc[p], 0, 0, 0);
}

AF4 SpdLoad(ASU2 tex, AU1 slice)
{
    return (AF4)0; // mip 5, not used
}

void SpdStore(ASU2 pix, AF4 outValue, AU1 index, AU1 slice)
{    
    RWTexture2D<float> imgDst = ResourceDescriptorHeap[c_outputSHTexture[index].x];
    imgDst[pix] = outValue.x;
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
    return AF4(spdIntermediate[x][y], 0, 0, 0);
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
    spdIntermediate[x][y] = value.x;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
    return (v0 + v1 + v2 + v3) * 0.25;
}

#define SPD_NO_WAVE_OPERATIONS
#include "../ffx_spd.h"

[numthreads(256, 1, 1)]
void main(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
    SpdDownsample(
        AU2(WorkGroupId.xy),
        AU1(LocalThreadIndex),
        AU1(c_mips),
        AU1(c_numWorkGroups),
        AU1(WorkGroupId.z),
        AU2(c_workGroupOffset));
}
