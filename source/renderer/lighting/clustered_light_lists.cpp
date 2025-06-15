#include "clustered_light_lists.h"
#include "../renderer.h"
#include "utils/profiler.h"
#include "utils/parallel_for.h"
#include "EASTL/atomic.h"

static const uint32_t tileSize = 48;
static const uint32_t sliceCount = 16;
static const float maxSliceDepth = 500.0f;

inline float4 GetLightBoudingSphere(const LocalLightData& light)
{
    switch ((LocalLightType)light.type)
    {
    case LocalLightType::Point:
        return float4(light.position, light.radius);
    case LocalLightType::Spot:
        return ConeBoundingSphere(light.position, -light.direction, light.radius, light.spotAngles.z);
    default:
        RE_ASSERT(false);
        break;
    }

    return float4();
}

inline float GetSliceDepth(uint32_t sliceIndex, uint32_t sliceCount, float zNear, float zFar)
{
    return zNear * powf(zFar / zNear, (float)sliceIndex / (float)sliceCount);
}

inline float3 ToViewSpace(float2 screenPos, uint32_t width, uint32_t height, const float4x4& mtxInvProjection)
{
    float2 screenUV = float2(screenPos) / float2(width, height);
    float2 ndcPos = (screenUV * 2.0f - 1.0f) * float2(1.0f, -1.0f);

    float3 viewPos = mul(mtxInvProjection, float4(ndcPos, 0.0, 1.0)).xyz();
    return viewPos;
}

inline float3 LineIntersectionWithZPlane(float3 start, float3 end, float zPlane)
{
    float3 direction = end - start;
    float3 normal = float3(0.0f, 0.0f, 1.0f);

    float t = (zPlane - dot(normal, start)) / dot(normal, direction);
    return start + t * direction;
}

inline bool TestSphereAABB(float3 position, float radius, float3 aabbMin, float3 aabbMax)
{
    float3 closestPoint = clamp(position, aabbMin, aabbMax);
    float distanceSquared = dot(closestPoint - position, closestPoint - position);
    return distanceSquared <= radius * radius;
}

ClusteredLightLists::ClusteredLightLists(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

ClusteredLightLists::~ClusteredLightLists()
{
}

void ClusteredLightLists::Build(uint32_t width, uint32_t height)
{
    CPU_EVENT("Render", "ClusteredLightLists::Build");

    const uint32_t lightCount = m_pRenderer->GetLocalLightCount();
    const LocalLightData* lights = m_pRenderer->GetLocalLights();
    const Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    struct ViewSpaceLight
    {
        float3 position;
        float radius;
        uint lightIndex;
    };
    
    eastl::vector<ViewSpaceLight> viewSpaceLights(lightCount);
    eastl::atomic<uint32_t> viewSpaceLightCount = 0;

    // cull lights & transform to view space
    ParallelFor(lightCount, [&](uint32_t index)
        {
            float4 boudingSphere = GetLightBoudingSphere(lights[index]);

            if (FrustumCull(camera->GetFrustumPlanes(), 6, boudingSphere.xyz(), boudingSphere.w))
            {
                ViewSpaceLight viewSpaceLight;
                viewSpaceLight.position = mul(camera->GetViewMatrix(), float4(boudingSphere.xyz(), 1.0)).xyz();
                viewSpaceLight.radius = boudingSphere.w;
                viewSpaceLight.lightIndex = index;

                uint32_t viewSpaceLightIndex = viewSpaceLightCount.fetch_add(1);
                viewSpaceLights[viewSpaceLightIndex] = viewSpaceLight;
            }
        });

    viewSpaceLights.resize(viewSpaceLightCount);


    const uint32_t tileCountX = DivideRoudingUp(width, tileSize);
    const uint32_t tileCountY = DivideRoudingUp(height, tileSize);
    const uint32_t tilesPerSlice = tileCountX * tileCountY;
    const uint32_t cellCount = tilesPerSlice * sliceCount;

    struct Cluster
    {
        float3 aabbMin;
        float3 aabbMax;
        eastl::vector<uint32_t> lightIndices;
    };
    eastl::vector<Cluster> clusters(cellCount);

    eastl::atomic<uint32_t> lightIndicesCount = 0;
    float4x4 mtxInvProjection = inverse(camera->GetProjectionMatrix());

    // build the cluster grid & assign lights to clusters
    ParallelFor(cellCount, [&](uint32_t index)
        {
            uint32_t sliceIndex = index / tilesPerSlice;
            uint32_t tileIndex = index % tilesPerSlice;
            uint32_t tileX = tileIndex % tileCountX;
            uint32_t tileY = tileIndex / tileCountX;

            float2 screenSpaceTileMin = float2(tileX, tileY) * tileSize;
            float2 screenSpaceTileMax = float2(tileX + 1, tileY + 1) * tileSize;
            
            float3 viewSpaceTileMin = ToViewSpace(screenSpaceTileMin, width, height, mtxInvProjection);
            float3 viewSpaceTileMax = ToViewSpace(screenSpaceTileMax, width, height, mtxInvProjection);

            float tileNearDepth = GetSliceDepth(sliceIndex, sliceCount, camera->GetZNear(), maxSliceDepth);
            float tileFarDepth = GetSliceDepth(sliceIndex + 1, sliceCount, camera->GetZNear(), maxSliceDepth);

            float3 minPointNear = LineIntersectionWithZPlane(float3(0.0), viewSpaceTileMin, tileNearDepth);
            float3 minPointFar = LineIntersectionWithZPlane(float3(0.0), viewSpaceTileMin, tileFarDepth);
            float3 maxPointNear = LineIntersectionWithZPlane(float3(0.0), viewSpaceTileMax, tileNearDepth);
            float3 maxPointFar = LineIntersectionWithZPlane(float3(0.0), viewSpaceTileMax, tileFarDepth);

            Cluster& cluster = clusters[index];

            cluster.aabbMin = min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar));
            cluster.aabbMax = max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar));

            for (uint32_t i = 0; i < viewSpaceLights.size(); ++i)
            {
                if (TestSphereAABB(viewSpaceLights[i].position, viewSpaceLights[i].radius, cluster.aabbMin, cluster.aabbMax))
                {
                    cluster.lightIndices.push_back(viewSpaceLights[i].lightIndex);

                    lightIndicesCount++;
                }
            }
        });

    // compact the light lists
    eastl::vector<uint32_t> lightIndices(lightIndicesCount);
    eastl::vector<uint2> lightGrids(cellCount);

    eastl::atomic<uint32_t> lightIndicesOffset = 0;

    ParallelFor(cellCount, [&](uint32_t index)
        {
            Cluster& cluster = clusters[index];

            uint32_t lightCount = cluster.lightIndices.size();
            uint32_t offset = lightIndicesOffset.fetch_add(lightCount);

            lightGrids[index] = uint2(offset, lightCount);

            if (lightCount > 0)
            {
                memcpy(lightIndices.data() + offset, cluster.lightIndices.data(), sizeof(uint32_t) * lightCount);
            }
        });

    m_lightGridBufferAddress = m_pRenderer->AllocateSceneConstant(lightGrids.data(), sizeof(uint2) * lightGrids.size());
    m_lightIndicesBufferAddress = m_pRenderer->AllocateSceneConstant(lightIndices.data(), sizeof(uint32_t) * lightIndices.size());
}
