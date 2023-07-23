#include "common.hlsli"

#define VA_SATURATE saturate
#include "XeGTAO.h"
#include "XeGTAO.hlsli"

ConstantBuffer<GTAOConstants> gtaoCB : register(b1);

cbuffer filterDepthCB : register(b0)
{
    uint c_srcRawDepth;
    uint c_outWorkingDepthMIP0;
    uint c_outWorkingDepthMIP1;
    uint c_outWorkingDepthMIP2;

    uint c_outWorkingDepthMIP3;
    uint c_outWorkingDepthMIP4;
}

[numthreads(8, 8, 1)] // <- hard coded to 8x8; each thread computes 2x2 blocks so processing 16x16 block: Dispatch needs to be called with (width + 16-1) / 16, (height + 16-1) / 16
void gtao_prefilter_depth_16x16(uint2 dispatchThreadID : SV_DispatchThreadID, uint2 groupThreadID : SV_GroupThreadID)
{
    Texture2D<float> srcRawDepth = ResourceDescriptorHeap[c_srcRawDepth];
    SamplerState samplerPointClamp = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    RWTexture2D<lpfloat> outWorkingDepthMIP0 = ResourceDescriptorHeap[c_outWorkingDepthMIP0];
    RWTexture2D<lpfloat> outWorkingDepthMIP1 = ResourceDescriptorHeap[c_outWorkingDepthMIP1];
    RWTexture2D<lpfloat> outWorkingDepthMIP2 = ResourceDescriptorHeap[c_outWorkingDepthMIP2];
    RWTexture2D<lpfloat> outWorkingDepthMIP3 = ResourceDescriptorHeap[c_outWorkingDepthMIP3];
    RWTexture2D<lpfloat> outWorkingDepthMIP4 = ResourceDescriptorHeap[c_outWorkingDepthMIP4];
    
    XeGTAO_PrefilterDepths16x16(dispatchThreadID, groupThreadID, gtaoCB, srcRawDepth, samplerPointClamp, outWorkingDepthMIP0, outWorkingDepthMIP1, outWorkingDepthMIP2, outWorkingDepthMIP3, outWorkingDepthMIP4);
}

cbuffer gtaoResource : register(b0)
{
    uint c_srcWorkingDepth;
    uint c_normalRT;
    uint c_hilbertLUT;
    uint c_outWorkingAOTerm;
    uint c_outWorkingEdges;
};

lpfloat2 SpatioTemporalNoise(uint2 pixCoord, uint temporalIndex)    // without TAA, temporalIndex is always 0
{
    float2 noise;
    
    Texture2D<uint> srcHilbertLUT = ResourceDescriptorHeap[c_hilbertLUT];
    uint index = srcHilbertLUT.Load(uint3(pixCoord % 64, 0)).x;

    index += 288 * (temporalIndex % 64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    return lpfloat2(frac(0.5 + index * float2(0.75487766624669276005, 0.5698402909980532659114)));
}

lpfloat3 LoadNormal(int2 pos)
{
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    float3 normal = DecodeNormal(normalRT.Load(int3(pos, 0)).xyz);
    
    normal = mul(GetCameraCB().mtxView, float4(normal, 0.0)).xyz;

    return (lpfloat3)normal;
}

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void gtao_main(const uint2 pixCoord : SV_DispatchThreadID)
{
    Texture2D<lpfloat> srcWorkingDepth = ResourceDescriptorHeap[c_srcWorkingDepth];
    SamplerState samplerPointClamp = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    RWTexture2D<uint> outWorkingAOTerm = ResourceDescriptorHeap[c_outWorkingAOTerm];
    RWTexture2D<unorm float> outWorkingEdges = ResourceDescriptorHeap[c_outWorkingEdges];
    
    if (srcWorkingDepth[pixCoord] == 0.0)
    {
        XeGTAO_OutputWorkingTerm(pixCoord, 1.0, float3(0, 0, 0), outWorkingAOTerm);
        outWorkingEdges[pixCoord] = 1.0;
        return;
    }

#if QUALITY_LEVEL == 0
    int sliceCount = 1;
    int stepsPerSlice = 2;
#elif QUALITY_LEVEL == 1
    int sliceCount = 2;
    int stepsPerSlice = 2;
#elif QUALITY_LEVEL == 2
    int sliceCount = 3;
    int stepsPerSlice = 3;
#else
    int sliceCount = 9;
    int stepsPerSlice = 3;
#endif

    XeGTAO_MainPass(pixCoord, sliceCount, stepsPerSlice, SpatioTemporalNoise(pixCoord, gtaoCB.NoiseIndex), LoadNormal(pixCoord), gtaoCB, srcWorkingDepth, samplerPointClamp, outWorkingAOTerm, outWorkingEdges);
}

cbuffer denoiseCB : register(b0)
{
    uint c_srcWorkingAOTerm;
    uint c_srcWorkingEdges;
    uint c_outFinalAOTerm;
};

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void gtao_denoise(const uint2 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<uint> srcWorkingAOTerm = ResourceDescriptorHeap[c_srcWorkingAOTerm];
    Texture2D<lpfloat> srcWorkingEdges = ResourceDescriptorHeap[c_srcWorkingEdges];
    SamplerState samplerPointClamp = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    RWTexture2D<uint> outFinalAOTerm = ResourceDescriptorHeap[c_outFinalAOTerm];
    
    const uint2 pixCoordBase = dispatchThreadID * uint2(2, 1); // we're computing 2 horizontal pixels at a time (performance optimization)

    XeGTAO_Denoise(pixCoordBase, gtaoCB, srcWorkingAOTerm, srcWorkingEdges, samplerPointClamp, outFinalAOTerm, true);
}