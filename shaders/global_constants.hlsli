#pragma once

struct SceneConstant
{
    uint sceneConstantBufferSRV;
    uint sceneStaticBufferSRV;
    uint sceneAnimationBufferSRV;
    uint sceneAnimationBufferUAV;

    uint instanceDataAddress;
    uint sceneRayTracingTLAS;
    uint secondPhaseMeshletsListUAV;
    uint secondPhaseMeshletsCounterUAV;

    uint bShowMeshlets;
    float3 lightDir;
    
    float3 lightColor;
    float lightRadius;
    
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
    uint debugFontCharBufferSRV;

    uint debugTextCounterBufferUAV;
    uint debugTextBufferUAV;
    uint bEnableStats;
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
    uint linearBlackBoarderSampler;
    uint preintegratedGFTexture;

    uint sobolSequenceTexture;
    uint scramblingRankingTexture1SPP;
    uint scramblingRankingTexture2SPP;
    uint scramblingRankingTexture4SPP;

    uint scramblingRankingTexture8SPP;
    uint scramblingRankingTexture16SPP;
    uint scramblingRankingTexture32SPP;
    uint scramblingRankingTexture64SPP;

    uint scramblingRankingTexture128SPP;
    uint scramblingRankingTexture256SPP;
    float frameTime;
    uint frameIndex;

    uint skyCubeTexture;
    uint skySpecularIBLTexture;
    uint skyDiffuseIBLTexture;
};

struct CullingData
{
    float3 viewPos;
    float _padding;
    
    float4 planes[6];
};

struct PhysicalCamera
{
    float aperture;
    float shutterSpeed;
    int iso;
    float exposureCompensation;

    /* add more physical params when adding dof in the future
    float focalLength;
    float focusDistance;
    float bladeNum;
    */
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
    
    float4x4 mtxPrevViewProjection;
    float4x4 mtxPrevViewProjectionInverse;
    
    CullingData culling;
    PhysicalCamera physicalCamera;
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif