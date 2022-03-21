#include "../common.hlsli"

cbuffer cb : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_tileMetaDataBuffer;
};

float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return float2(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight); }
int2 FFX_DNSR_Shadows_GetBufferDimensions() { return int2(SceneCB.viewWidth, SceneCB.viewHeight); }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return CameraCB.mtxProjectionInverse; }
float FFX_DNSR_Shadows_GetDepthSimilaritySigma() { return 1.0; }

float FFX_DNSR_Shadows_ReadDepth(int2 p)
{
    Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
    return depthTexture[p].x;
}

float16_t3 FFX_DNSR_Shadows_ReadNormals(int2 p)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return (float16_t3)OctNormalDecode(normalTexture[p].xyz);
}

bool FFX_DNSR_Shadows_IsShadowReciever(uint2 p)
{
    float depth = FFX_DNSR_Shadows_ReadDepth(p);
    return (depth > 0.0f) && (depth < 1.0f);
}

float16_t2 FFX_DNSR_Shadows_ReadInput(int2 p)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    return (float16_t2)inputTexture[p].xy;
}

uint FFX_DNSR_Shadows_ReadTileMetaData(uint p)
{
    StructuredBuffer<uint> tileMetaDataBuffer = ResourceDescriptorHeap[c_tileMetaDataBuffer];
    return tileMetaDataBuffer[p];
}

#include "ffx-shadows-dnsr/ffx_denoiser_shadows_filter.h"

[numthreads(8, 8, 1)]
void main(uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint2 did : SV_DispatchThreadID)
{
#if FILTER_PASS == 0
    const uint PASS_INDEX = 0;
    const uint STEP_SIZE = 1;
#elif FILTER_PASS == 1
    const uint PASS_INDEX = 1;
    const uint STEP_SIZE = 2;
#elif FILTER_PASS == 2
    const uint PASS_INDEX = 2;
    const uint STEP_SIZE = 4;
#endif

    bool bWriteOutput = false;
    float2 results = FFX_DNSR_Shadows_FilterSoftShadowsPass(gid, gtid, did, bWriteOutput, PASS_INDEX, STEP_SIZE);

    if (bWriteOutput)
    {
#if FILTER_PASS == 2
        // Recover some of the contrast lost during denoising
        const float shadow_remap = max(1.2f - results.y, 1.0f);
        const float mean = pow(abs(results.x), shadow_remap);

        RWTexture2D<unorm float> outputTexture = ResourceDescriptorHeap[c_outputTexture];
        outputTexture[did] = mean;
#else
        RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[c_outputTexture];
        outputTexture[did] = results;
#endif
    }
}