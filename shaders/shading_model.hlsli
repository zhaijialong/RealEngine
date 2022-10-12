#pragma once

enum class ShadingModel : uint
{
    Default,
    Anisotropy,
    Sheen,
    ClearCoat,
    Hair,
    //SSS,
    
    Max,
};

inline float EncodeShadingModel(ShadingModel shadingModel)
{
    return (float)shadingModel / 255.0f;
}

inline ShadingModel DecodeShadingModel(float shadingModel)
{
    return (ShadingModel)round(shadingModel * 255.0);
}