#pragma once

struct Bilinear
{
    float2 origin;
    float2 weights;
};

Bilinear GetBilinearFilter(float2 uv, float2 texSize)
{
    Bilinear result;
    result.origin = floor(uv * texSize - 0.5);
    result.weights = frac(uv * texSize - 0.5);
    return result;
}

float4 GetBilinearCustomWeights(Bilinear f, float4 customWeights)
{
    float4 weights;
    weights.x = (1.0 - f.weights.x) * (1.0 - f.weights.y);
    weights.y = f.weights.x * (1.0 - f.weights.y);
    weights.z = (1.0 - f.weights.x) * f.weights.y;
    weights.w = f.weights.x * f.weights.y;
    return weights * customWeights;
}

template<typename T>
T ApplyBilinearCustomWeights(T s00, T s10, T s01, T s11, float4 w, bool normalize = true)
{
    T r = s00 * w.x + s10 * w.y + s01 * w.z + s11 * w.w;
    return r * (normalize ? rcp(max(0.00001, dot(w, 1.0))) : 1.0);
}