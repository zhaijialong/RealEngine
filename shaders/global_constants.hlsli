#pragma once

struct SceneConstant
{
    float3 lightDir;
    float _padding0;
    
    float3 lightColor;
    float _padding1;
};

struct CameraConstant
{
    float3 cameraPos;
};

#ifndef __cplusplus
ConstantBuffer<CameraConstant> CameraCB : register(b3);
ConstantBuffer<SceneConstant> SceneCB : register(b4);
#endif