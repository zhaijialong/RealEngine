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
    struct HierarchicalTracingRay
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
    
    bool CastRay(HierarchicalTracingRay ray, out HitInfo hitInfo)
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
    
    struct LinearTracingRay
    {
        float3 origin;
        float3 direction;
        float linearSteps;
        float bisectionStep;
        float jitter;
        Texture2D<float> depthTexture;
    };
    
    struct IntersectionResult
    {
        bool intersected;
        float depth;
    };
    
    IntersectionResult IntersectionTest(LinearTracingRay ray, float3 rayPointNdc)
    {
        const float2 uv = GetScreenUV(rayPointNdc.xy);
        const float rayDepth = rayPointNdc.z;
        const float linearRayDepth = GetLinearDepth(rayDepth);
        
        SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
        const float depth = ray.depthTexture.SampleLevel(pointSampler, uv, 0);
        const float linearDepth = GetLinearDepth(depth);

        const float thickness = 0.3;
        const float penetration = linearRayDepth / linearDepth - 1.0;
        
        IntersectionResult res;
        res.intersected = (rayDepth < depth) && (penetration < thickness);
        res.depth = depth;

        return res;
    }
    
    bool CastRay(LinearTracingRay ray, out HitInfo hitInfo)
    {
        float4 rayStartClip = mul(GetCameraCB().mtxViewProjection, float4(ray.origin, 1));
        float4 rayEndClip = mul(GetCameraCB().mtxViewProjection, float4(ray.origin + ray.direction, 1));
        
        float3 rayStartNdc = clamp(GetNdcPosition(rayStartClip), -1, 1);
        float3 rayEndNdc = clamp(GetNdcPosition(rayEndClip), -1, 1);
        
        {
            float3 direction = rayEndNdc - rayStartNdc;
        
            // Clip the ray to the frustum
            const float2 distToEdge = (sign(direction.xy) - rayStartNdc.xy) / direction.xy;        
            rayEndNdc = rayStartNdc + direction * min(distToEdge.x, distToEdge.y);
        }        
        
        
        float minT = 0.0;
        float maxT = 1.0;
        
        const float3 rayDirectionNdc = rayEndNdc - rayStartNdc;
        const float stepT = (maxT - minT) / ray.linearSteps;
        
        bool intersected = false;

        for (uint step = 0; step < ray.linearSteps; ++step)
        {
            const float candidateT = minT + stepT * (step == 0 ? ray.jitter : 1.0);
            const float3 candidate = rayStartNdc + rayDirectionNdc * candidateT;

            const IntersectionResult result = IntersectionTest(ray, candidate);
            intersected = result.intersected;

            if (intersected)
            {
                maxT = candidateT;
                hitInfo.depth = result.depth;
                break;
            }
            else
            {
                minT = candidateT;
            }
        }
        
        if(intersected)
        {
            for (uint step = 0; step < ray.bisectionStep; ++step)
            {
                const float midT = (minT + maxT) * 0.5;
                const float3 candidate = rayStartNdc + rayDirectionNdc * midT;

                const IntersectionResult result = IntersectionTest(ray, candidate);

                if (result.intersected)
                {
                    maxT = midT;
                    hitInfo.depth = result.depth;
                }
                else
                {
                    minT = midT;
                }
            }
            
            hitInfo.screenUV = lerp(GetScreenUV(rayStartNdc.xy), GetScreenUV(rayEndNdc.xy), maxT);
            return true;
        }
        
        return false;
    }
}