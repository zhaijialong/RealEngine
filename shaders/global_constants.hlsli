#pragma once

struct SceneConstant
{
    float3 lightDir;
    uint shadowRT;
    
    float3 lightColor;
    uint shadowSampler;
    
    float4x4 mtxLightVP;
};

struct CameraConstant
{
    float3 cameraPos;
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif