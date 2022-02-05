#pragma once

#include "gpu_scene.hlsli"
#include "model.hlsli"

namespace rt
{
    float3 GetBarycentricCoordinates(float2 barycentrics)
    {
        return float3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
    }

    bool AlphaTest(uint instanceID, uint primitiveIndex, float3 barycentricCoordinates)
    {
        InstanceData instanceData = GetInstanceData(instanceID);
        if (instanceData.GetInstanceType() == InstanceType::Model)
        {
            uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveIndex);

            float2 uv0 = LoadSceneStaticBuffer<float2>(instanceData.uvBufferAddress, primitiveIndices.x);
            float2 uv1 = LoadSceneStaticBuffer<float2>(instanceData.uvBufferAddress, primitiveIndices.y);
            float2 uv2 = LoadSceneStaticBuffer<float2>(instanceData.uvBufferAddress, primitiveIndices.z);

            float2 uv = uv0 * barycentricCoordinates.x + uv1 * barycentricCoordinates.y + uv2 * barycentricCoordinates.z;

            ModelMaterialConstant material = model::GetMaterialConstant(instanceID);

            Texture2D texture = model::GetMaterialTexture2D(material.albedoTexture != INVALID_RESOURCE_INDEX ? material.albedoTexture : material.diffuseTexture);
            SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearRepeatSampler];

            float alpha = texture.SampleLevel(linearSampler, uv, 0).a;
            return alpha > material.alphaCutoff;
        }

        return true;
    }

    float VisibilityRay(float3 rayOrigin, float3 rayDirection, float rayTMax)
    {
        RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[SceneCB.sceneRayTracingTLAS];

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = rayDirection;
        ray.TMin = 0.0;
        ray.TMax = rayTMax;

        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
        q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, ray);

        while (q.Proceed())
        {
            uint instanceID = q.CandidateInstanceID();
            uint primitiveIndex = q.CandidatePrimitiveIndex();
            float3 barycentricCoordinates = GetBarycentricCoordinates(q.CandidateTriangleBarycentrics());

            if (rt::AlphaTest(instanceID, primitiveIndex, barycentricCoordinates))
            {
                q.CommitNonOpaqueTriangleHit();
            }
        }

        return q.CommittedStatus() == COMMITTED_NOTHING ? 1.0 : 0.0;
    }
}