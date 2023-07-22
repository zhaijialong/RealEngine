#pragma once

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
    float spreadAngle;
    
    float nearZ;
    float _padding;
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
	float4x4 mtxClipToPrevViewNoJitter;
    
    float4x4 mtxPrevView;
    float4x4 mtxPrevViewProjection;
    float4x4 mtxPrevViewProjectionInverse;
    
    CullingData culling;
    PhysicalCamera physicalCamera;
};

struct SceneConstant
{
    CameraConstant cameraCB;

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

    uint2 renderSize;
    float2 rcpRenderSize;

    uint2 displaySize;
    float2 rcpDisplaySize;

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
    uint bilinearRepeatSampler;
    uint bilinearClampSampler;

    uint bilinearBlackBoarderSampler;
    uint bilinearWhiteBoarderSampler;
    uint trilinearRepeatSampler;
    uint trilinearClampSampler;

    uint aniso2xSampler;
    uint aniso4xSampler;
    uint aniso8xSampler;
    uint aniso16xSampler;

    uint minReductionSampler;
    uint maxReductionSampler;
    uint blueNoiseTexture;
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

    float mipBias;
    uint skyCubeTexture;
    uint skySpecularIBLTexture;
    uint skyDiffuseIBLTexture;

    uint sheenETexture;
    uint tonyMcMapfaceTexture;
    uint marschnerTextureM;
    uint marschnerTextureN;
};

#ifndef __cplusplus
ConstantBuffer<SceneConstant> SceneCB : register(b2);

CameraConstant GetCameraCB()
{
    return SceneCB.cameraCB;
}
#endif