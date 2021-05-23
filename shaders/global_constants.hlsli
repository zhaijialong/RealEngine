#pragma once

struct SceneConstant
{
    float3 lightDir;
    uint shadowRT;
    
    float3 lightColor;
    float _padding1;
    
    float4x4 mtxlightVP;
};

struct CameraConstant
{
    float3 cameraPos;
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif