#pragma once

struct MaterialTextureInfo
{
    uint index;
    uint width : 16;
    uint height : 16;
    uint bTransform;
    float rotation;
    
    float2 offset;
    float2 scale;
    
#ifdef __cplusplus
    MaterialTextureInfo()
    {
        index = GFX_INVALID_RESOURCE;
        width = height = 0;
        bTransform = false;
        rotation = 0.0f;
    }
#else
    float2 TransformUV(float2 uv)
    {
        if (bTransform)
        {
            float3x3 mtxTranslation = float3x3(1, 0, 0, 0, 1, 0, offset.x, offset.y, 1);
            float3x3 mtxRotation = float3x3(cos(rotation), sin(rotation), 0, -sin(rotation), cos(rotation), 0, 0, 0, 1);
            float3x3 mtxScale = float3x3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, 1);

            return mul(mul(mul(float3(uv, 1), mtxScale), mtxRotation), mtxTranslation).xy;
        }

        return uv;
    };
    
    // "Texture Level-of-Detail Strategies for Real-Time Ray Tracing"
    float ComputeTextureLOD(float3 rayDirection, float3 surfaceNormal, float rayConeWidth, float triangleLODConstant)
    {        
        return triangleLODConstant + 0.5 * log2(width * height) + log2(abs(rayConeWidth / dot(rayDirection, surfaceNormal))); // Eq. 34
    }
#endif
};
