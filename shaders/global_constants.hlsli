#pragma once

struct SceneConstant
{
    uint sceneBufferSRV;
    uint sceneConstantBufferSRV;
    uint instanceDataAddress;
    uint _padding0;

    uint occlusionCulledMeshletsBufferUAV;
    uint occlusionCulledMeshletsCounterBufferUAV;
    uint occlusionCulledMeshletsBufferSRV;
    uint occlusionCulledMeshletsCounterBufferSRV;

    float3 lightDir;
    uint shadowRT;
    
    float3 lightColor;
    uint shadowSampler;
    
    float4x4 mtxLightVP;
    
    uint viewWidth;
    uint viewHeight;
    float rcpViewWidth;
    float rcpViewHeight;
    
    uint HZBWidth;
    uint HZBHeight;
    uint firstPhaseCullingHZBSRV;
    uint secondPhaseCullingHZBSRV;
    
    uint sceneHZBSRV;
    uint debugLineDrawCommandUAV;
    uint debugLineVertexBufferUAV;
    uint debugLineVertexBufferSRV;
    
    uint debugTextCounterBufferUAV;
    uint debugTextBufferUAV;
    uint debugFontCharBufferSRV;
    uint statsBufferUAV;
    
    uint pointRepeatSampler;
    uint pointClampSampler;
    uint linearRepeatSampler;
    uint linearClampSampler;
    
    uint aniso2xSampler;
    uint aniso4xSampler;
    uint aniso8xSampler;
    uint aniso16xSampler;
    
    uint minReductionSampler;
    uint maxReductionSampler;
    uint envTexture;
    uint brdfTexture;
};

struct CullingData
{
    float3 viewPos;
    float _padding;
    
    float4 planes[6];
};

struct CameraConstant
{
    float3 cameraPos;
    uint _padding;
    
    float nearZ;
    float farZ;
    float2 linearZParams;

    float2 jitter;
    float2 prevJitter;
    
    float4x4 mtxView;
    float4x4 mtxViewInverse;
    float4x4 mtxProjection;
    float4x4 mtxProjectionInverse;
    float4x4 mtxViewProjection;
    float4x4 mtxViewProjectionInverse;
    
    float4x4 mtxViewProjectionNoJitter;
    float4x4 mtxPrevViewProjectionNoJitter;
    float4x4 mtxClipToPrevClipNoJitter;
    
    float4x4 mtxPrevViewProjectionInverse;
    
    CullingData culling;
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif