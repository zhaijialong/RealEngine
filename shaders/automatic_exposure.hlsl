#include "common.hlsli"
#include "exposure.hlsli"
#include "debug.hlsli"

cbuffer InitLuminanceConstants : register(b1)
{
    uint c_inputSRV;
    uint c_outputUAV;
    float c_width;
    float c_height;
    float c_rcpWidth;
    float c_rcpHeight;
    float c_minLuminance;
    float c_maxLuminance;
};

[numthreads(8, 8, 1)]
void init_luminance(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputSRV];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

    float2 screenPos = (float2)dispatchThreadID.xy + 0.5;
    float2 uv = screenPos * float2(c_rcpWidth, c_rcpHeight);
    float3 color = inputTexture.SampleLevel(linearSampler, uv, 0).xyz;

    float luminance = clamp(Luminance(color), c_minLuminance, c_maxLuminance);
    float weight = GetMeteringWeight(screenPos, float2(c_width, c_height));
    
    RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[c_outputUAV];
    outputTexture[dispatchThreadID.xy] = float2(luminance, weight);
}

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
    SamplerState srcSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    AF2 textureCoord = p * c_invInputSize + c_invInputSize;
    AF4 result = imgSrc.SampleLevel(srcSampler, textureCoord, 0);
    result = AF4(result.xy, 0, 0);
    return result;
}
AF4 SpdLoad(ASU2 tex, AU1 slice)
{
    globallycoherent RWTexture2D<float2> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
    return float4(imgDst5[tex], 0, 0);
}
void SpdStore(ASU2 pix, AF4 outValue, AU1 index, AU1 slice)
{
    if (index == 5)
    {
        globallycoherent RWTexture2D<float2> imgDst5 = ResourceDescriptorHeap[c_imgDst[5].x];
        imgDst5[pix] = outValue.xy;
        return;
    }
    
    RWTexture2D<float2> imgDst = ResourceDescriptorHeap[c_imgDst[index].x];
    imgDst[pix] = outValue.xy;
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
    return AF4(spdIntermediateR[x][y], spdIntermediateG[x][y], 0, 0);
}
void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
}
AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
    float weight = (v0.y + v1.y + v2.y + v3.y) * 0.25;
    float luminance = 0.0f;
    if(weight > 0.0)
    {
        luminance = (v0.x * v0.y + v1.x * v1.y + v2.x * v2.y + v3.x * v3.y) / (v0.y + v1.y + v2.y + v3.y);
    }
    return float4(luminance, weight, 0, 0);
}

#define SPD_LINEAR_SAMPLER
#include "ffx_spd.h"

[numthreads(256, 1, 1)]
void luminance_reduction(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
    SpdDownsample(
        AU2(WorkGroupId.xy),
        AU1(LocalThreadIndex),
        AU1(c_mips),
        AU1(c_numWorkGroups),
        AU1(WorkGroupId.z),
        AU2(c_workGroupOffset));
}

cbuffer ExpsureConstants : register(b0)
{
    uint c_avgLuminanceTexture;
    uint c_avgLuminanceMip;
    uint c_exposureTexture;
    uint c_previousEV100Texture;
    float c_adaptionSpeed;
};

[numthreads(1, 1, 1)]
void exposure()
{
#if EXPOSURE_MODE_AUTO || EXPOSURE_MODE_AUTO_HISTOGRAM
    Texture2D<float2> avgLuminanceTexture = ResourceDescriptorHeap[c_avgLuminanceTexture];
    float avgLuminance = avgLuminanceTexture.Load(uint3(0, 0, c_avgLuminanceMip)).x;

    float EV100 = ComputeEV100(max(avgLuminance, 0.001));
    
    RWTexture2D<float> previousEV100Texture = ResourceDescriptorHeap[c_previousEV100Texture];
    float previousEV100 = previousEV100Texture[uint2(0, 0)];

    //eye adaption
    EV100 = previousEV100 + (EV100 - previousEV100) * (1 - exp(-SceneCB.frameTime * c_adaptionSpeed));
    previousEV100Texture[uint2(0, 0)] = EV100;
#else
    float EV100 = ComputeEV100(GetCameraCB().physicalCamera.aperture, GetCameraCB().physicalCamera.shutterSpeed, GetCameraCB().physicalCamera.iso);
#endif

#if DEBUG_SHOW_EV100
    float2 pos = float2(100, 100);
    float3 color = float3(1, 1, 1);
    debug::PrintString(pos, color, 'E', 'V', '1', '0', '0', ':', ' ');
    debug::PrintFloat(pos, color, EV100);
#endif

    float exposure = ComputeExposure(EV100 - GetCameraCB().physicalCamera.exposureCompensation);

    RWTexture2D<float> exposureTexture = ResourceDescriptorHeap[c_exposureTexture];
    exposureTexture[uint2(0, 0)] = exposure;
}