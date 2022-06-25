#pragma once

enum class ShadingModel : uint
{
    Default,
    Anisotropy,
    
    Max,
};

#ifndef __cplusplus
float EncodeShadingModel(ShadingModel shadingModel)
{
    return (float)shadingModel / 255.0f;
}

ShadingModel DecodeShadingModel(float shadingModel)
{
    return (ShadingModel)round(shadingModel * 255.0);
}
#endif //#ifndef __cplusplus