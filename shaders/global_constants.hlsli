#pragma once

struct SceneConstant
{
    float3 lightDir;
    uint shadowRT;
    
    float3 lightColor;
    uint shadowSampler;
    
    float4x4 mtxLightVP;
    
    uint viewWidth;
    uint viewHeight;
    float rcpViewWidth;
    float rcpViewHeight;
    
    uint pointRepeatSampler;
    uint pointClampSampler;
    uint linearRepeatSampler;
    uint linearClampSampler;
    
    uint aniso2xSampler;
    uint aniso4xSampler;
    uint aniso8xSampler;
    uint aniso16xSampler;
    
    uint envTexture;
    uint brdfTexture;
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
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif