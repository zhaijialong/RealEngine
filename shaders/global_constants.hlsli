#pragma once

struct SceneConstant
{
    
};

struct CameraConstant
{
    
};

#ifndef __cplusplus
ConstantBuffer<SceneConstant> SceneCB : register(b3);
ConstantBuffer<CameraConstant> CameraCB : register(b4);
#endif