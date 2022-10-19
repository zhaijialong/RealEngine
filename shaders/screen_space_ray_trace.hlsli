#include "common.hlsli"

float FFX_SSSR_LoadDepth(int2 pixel_coordinate, int mip)
{
    Texture2D hzbTexture = ResourceDescriptorHeap[SceneCB.sceneHZBSRV];
    return hzbTexture.Load(int3(pixel_coordinate, mip)).y;
}

#define FFX_SSSR_INVERTED_DEPTH_RANGE 1
#include "ffx_sssr.h"

namespace ssrt
{
    struct Ray
    {
        float3 origin;
        float3 direction;
        float maxInteration;
    };
    
    struct HitInfo
    {
        float2 screenUV;
        float depth;
    };
    
    bool HierarchicalRaymarch(Ray ray, out HitInfo hitInfo)
    {
        float4 rayStartClip = mul(GetCameraCB().mtxViewProjection, float4(ray.origin, 1));
        float4 rayEndClip = mul(GetCameraCB().mtxViewProjection, float4(ray.origin + ray.direction, 1));
        
        float3 rayStartNdc = clamp(GetNdcPosition(rayStartClip), -1, 1);
        float3 rayEndNdc = clamp(GetNdcPosition(rayEndClip), -1, 1);
        
        float3 rayStartScreen = float3(GetScreenUV(rayStartNdc.xy), rayStartNdc.z);
        float3 rayEndScreen = float3(GetScreenUV(rayEndNdc.xy), rayEndNdc.z);
        
        float3 rayDirectionScreen = rayEndScreen - rayStartScreen;
        
        float2 screen_size = float2(SceneCB.HZBWidth, SceneCB.HZBHeight);
        
        bool valid_hit;
        float3 hit = FFX_SSSR_HierarchicalRaymarch(rayStartScreen, rayDirectionScreen, false, screen_size, 0, 4, ray.maxInteration, valid_hit);
        
        hitInfo.screenUV = hit.xy;
        hitInfo.depth = hit.z;
        
        if (any(hit.xy < 0) || any(hit.xy > 1))
        {
            valid_hit = false;
        }
        
        return valid_hit;
    }
}