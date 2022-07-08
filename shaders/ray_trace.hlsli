#pragma once

#include "gpu_scene.hlsli"
#include "brdf.hlsli"

#define DYNAMIC_MATERIAL_SWITCH
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
            model::Primitive primitive = model::Primitive::Create(instanceID, primitiveIndex);
            float2 uv = primitive.GetUV(barycentricCoordinates);

            ModelMaterialConstant material = model::GetMaterialConstant(instanceID);

            Texture2D texture = model::GetMaterialTexture2D(material.albedoTexture != INVALID_RESOURCE_INDEX ? material.albedoTexture : material.diffuseTexture);
            SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearRepeatSampler];

            float alpha = texture.SampleLevel(linearSampler, uv, 0).a;
            return alpha > material.alphaCutoff;
        }

        return true;
    }

    bool TraceVisibilityRay(RayDesc ray)
    {
        RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[SceneCB.sceneRayTracingTLAS];

        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
        q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, ray);

        while (q.Proceed())
        {
            uint instanceID = q.CandidateInstanceID();
            uint primitiveIndex = q.CandidatePrimitiveIndex();
            float3 barycentricCoordinates = GetBarycentricCoordinates(q.CandidateTriangleBarycentrics());

            if (AlphaTest(instanceID, primitiveIndex, barycentricCoordinates))
            {
                q.CommitNonOpaqueTriangleHit();
            }
        }

        bool visible = q.CommittedStatus() == COMMITTED_NOTHING;
        return visible;
    }

    struct HitInfo
    {
        float3 position;
        float rayT;
        float3 barycentricCoordinates;
        uint instanceID;
        uint primitiveIndex;
        bool bFrontFace;
    };

    bool TraceRay(RayDesc ray, out HitInfo hitInfo)
    {
        RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[SceneCB.sceneRayTracingTLAS];

        RayQuery<RAY_FLAG_NONE> q;
        q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, ray);

        while (q.Proceed())
        {
            uint instanceID = q.CandidateInstanceID();
            uint primitiveIndex = q.CandidatePrimitiveIndex();
            float3 barycentricCoordinates = GetBarycentricCoordinates(q.CandidateTriangleBarycentrics());

            if (AlphaTest(instanceID, primitiveIndex, barycentricCoordinates))
            {
                q.CommitNonOpaqueTriangleHit();
            }
        }

        if (q.CommittedStatus() == COMMITTED_NOTHING)
        {
            return false;
        }

        hitInfo.position = q.WorldRayOrigin() + q.WorldRayDirection() * q.CommittedRayT();
        hitInfo.rayT = q.CommittedRayT();
        hitInfo.barycentricCoordinates = GetBarycentricCoordinates(q.CommittedTriangleBarycentrics());
        hitInfo.instanceID = q.CommittedInstanceID();
        hitInfo.primitiveIndex = q.CommittedPrimitiveIndex();
        hitInfo.bFrontFace = q.CommittedTriangleFrontFace();

        return true;
    }

    struct MaterialData
    {
        float3 diffuse;
        float3 specular;
        float3 worldNormal;
        float roughness;
        float3 emissive;
    };

    MaterialData GetMaterial(HitInfo hitInfo) //todo : texture lod
    {
        MaterialData material = (MaterialData)0;
        InstanceData instanceData = GetInstanceData(hitInfo.instanceID);

        switch (instanceData.GetInstanceType())
        {
            case InstanceType::Model:
            {
                model::Primitive primitive = model::Primitive::Create(hitInfo.instanceID, hitInfo.primitiveIndex);
                ModelMaterialConstant modelMaterial = model::GetMaterialConstant(hitInfo.instanceID);
                
                float2 uv = primitive.GetUV(hitInfo.barycentricCoordinates);

                if (modelMaterial.bPbrMetallicRoughness)
                {
                    model::PbrMetallicRoughness pbrMetallicRoughness = model::GetMaterialMetallicRoughness(hitInfo.instanceID, uv);

                    material.diffuse = pbrMetallicRoughness.albedo * (1.0 - pbrMetallicRoughness.metallic);
                    material.specular = lerp(0.04, pbrMetallicRoughness.albedo, pbrMetallicRoughness.metallic);
                    material.roughness = pbrMetallicRoughness.roughness;
                }
                else if (modelMaterial.bPbrSpecularGlossiness)
                {
                    model::PbrSpecularGlossiness pbrSpecularGlossiness = model::GetMaterialSpecularGlossiness(hitInfo.instanceID, uv);

                    material.specular = pbrSpecularGlossiness.specular;
                    material.diffuse = pbrSpecularGlossiness.diffuse * (1.0 - max(max(material.specular.r, material.specular.g), material.specular.b));
                    material.roughness = 1.0 - pbrSpecularGlossiness.glossiness;
                }

                material.emissive = model::GetMaterialEmissive(hitInfo.instanceID, uv);
                
                float3 N = normalize(mul(instanceData.mtxWorldInverseTranspose, float4(primitive.GetNormal(hitInfo.barycentricCoordinates), 0.0)).xyz);
                if(modelMaterial.normalTexture != INVALID_RESOURCE_INDEX)
                {
                    float3 T = normalize(mul(instanceData.mtxWorldInverseTranspose, float4(primitive.GetTangent(hitInfo.barycentricCoordinates).xyz, 0.0)).xyz);
                    float3 B = normalize(cross(N, T) * primitive.GetTangent(hitInfo.barycentricCoordinates).w);

                    N = model::GetMaterialNormal(hitInfo.instanceID, uv, T, B, N);
                }

                if(modelMaterial.bDoubleSided)
                {
                    N *= hitInfo.bFrontFace ? 1.0 : -1.0;
                }
                
                material.worldNormal = N;

                break;
            }
            default:
                break;
        }

        return material;
    }
    
    float3 Shade(HitInfo hitInfo, MaterialData material, float3 V)
    {
        RayDesc ray;
        ray.Origin = hitInfo.position + material.worldNormal * 0.01;
        ray.Direction = SceneCB.lightDir;
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;
        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
        float3 direct_lighting = DefaultBRDF(SceneCB.lightDir, V, material.worldNormal, material.diffuse, material.specular, material.roughness) * visibility;
        
        float3 indirect_diffuse = DiffuseIBL(material.worldNormal) * material.diffuse;
        float3 indirect_specular = SpecularIBL(material.worldNormal, V, material.roughness, material.specular);

        float3 radiance = material.emissive + direct_lighting + indirect_diffuse + indirect_specular;
        return radiance;
    }

}