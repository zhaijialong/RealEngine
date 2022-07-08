#include "../common.hlsli"

cbuffer CB : register(b1)
{
    uint c_shadowMaskBuffer; //output of prepare pass
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_velocityTexture;
    uint c_historyTexture;
    uint c_prevLinearDepthTexture;
    uint c_prevMomentsTexture;
    uint c_momentsTexture; //RWTexture2D<float3>, r11g11b10f
    uint c_tileMetaDataBuffer; //RWStructuredBuffer<uint>
    uint c_reprojectionResultTexture; //RWTexture2D<float2>, r16g16f
    uint c_bFirstFrame;
};

float4x4 FFX_DNSR_Shadows_GetViewProjectionInverse() { return CameraCB.mtxViewProjectionInverse; }
float4x4 FFX_DNSR_Shadows_GetReprojectionMatrix() { return CameraCB.mtxProjection; }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return CameraCB.mtxProjectionInverse; }
float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return SceneCB.rcpRenderSize; }
int2 FFX_DNSR_Shadows_GetBufferDimensions() { return SceneCB.renderSize; }
int FFX_DNSR_Shadows_IsFirstFrame() { return c_bFirstFrame; }
float3 FFX_DNSR_Shadows_GetEye() { return CameraCB.cameraPos; }

float FFX_DNSR_Shadows_ReadDepth(int2 p)
{
    Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
    return depthTexture[p].x;
}

float FFX_DNSR_Shadows_ReadPreviousLinearDepth(int2 p)
{
    Texture2D prevLinearDepthTexture = ResourceDescriptorHeap[c_prevLinearDepthTexture];
    return prevLinearDepthTexture[p].x;
}

float3 FFX_DNSR_Shadows_ReadNormals(int2 p)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    return DecodeNormal(normalTexture[p].xyz);
}

float2 FFX_DNSR_Shadows_ReadVelocity(int2 p)
{
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
    return velocityTexture[p].xy;
}

float FFX_DNSR_Shadows_ReadHistory(float2 p)
{
    Texture2D historyTexture = ResourceDescriptorHeap[c_historyTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    return historyTexture.SampleLevel(linearSampler, p, 0).x;
}

float3 FFX_DNSR_Shadows_ReadPreviousMomentsBuffer(int2 p)
{
    Texture2D prevMomentsTexture = ResourceDescriptorHeap[c_prevMomentsTexture];
    return prevMomentsTexture[p].xyz;
}

uint FFX_DNSR_Shadows_ReadRaytracedShadowMask(uint p)
{
    StructuredBuffer<uint> shadowMaskBuffer = ResourceDescriptorHeap[c_shadowMaskBuffer];
    return shadowMaskBuffer[p];
}

void FFX_DNSR_Shadows_WriteMetadata(uint p, uint val)
{
    RWStructuredBuffer<uint> tileMetaDataBuffer = ResourceDescriptorHeap[c_tileMetaDataBuffer];
    tileMetaDataBuffer[p] = val;
}

void FFX_DNSR_Shadows_WriteMoments(uint2 p, float3 val)
{
    RWTexture2D<float3> momentsTexture = ResourceDescriptorHeap[c_momentsTexture];
    momentsTexture[p] = val;
}

void FFX_DNSR_Shadows_WriteReprojectionResults(uint2 p, float2 val)
{
    RWTexture2D<float2> reprojectionResultTexture = ResourceDescriptorHeap[c_reprojectionResultTexture];
    reprojectionResultTexture[p] = val;
}

bool FFX_DNSR_Shadows_IsShadowReciever(uint2 p)
{
    float depth = FFX_DNSR_Shadows_ReadDepth(p);
    return (depth > 0.0f) && (depth < 1.0f);
}

#include "ffx-shadows-dnsr/ffx_denoiser_shadows_tileclassification.h"

[numthreads(8, 8, 1)]
void main(uint group_index : SV_GroupIndex, uint2 gid : SV_GroupID)
{
    FFX_DNSR_Shadows_TileClassification(group_index, gid);
}