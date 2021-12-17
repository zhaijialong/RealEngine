#include "common.hlsli"
#include "model_constants.hlsli"
#include "debug.hlsli"
#include "stats.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_meshletCount;
    uint c_meshletBuffer;
    uint c_meshletVerticesBuffer;
    uint c_meshletIndicesBuffer;
};

struct Meshlet
{
    float3 center;
    float radius;
    
    uint cone; //axis + cutoff, rgba8snorm
    
    uint vertexCount;
    uint triangleCount;
    
    uint vertexOffset;
    uint triangleOffset;
};

struct Payload
{
    uint meshletIndices[32];
};

groupshared Payload s_Payload;

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool ProjectSphere(float3 center, float radius, float znear, float P00, float P11, out float4 aabb)
{
    if (center.z < radius + znear)
    {
        return false;
    }

    float2 cx = -center.xz;
    float2 vx = float2(sqrt(dot(cx, cx) - radius * radius), radius);
    float2 minx = mul(cx, float2x2(float2(vx.x, vx.y), float2(-vx.y, vx.x)));
    float2 maxx = mul(cx, float2x2(float2(vx.x, -vx.y), float2(vx.y, vx.x)));

    float2 cy = -center.yz;
    float2 vy = float2(sqrt(dot(cy, cy) - radius * radius), radius);
    float2 miny = mul(cy, float2x2(float2(vy.x, vy.y), float2(-vy.y, vy.x)));
    float2 maxy = mul(cy, float2x2(float2(vy.x, -vy.y), float2(vy.y, vy.x)));

    aabb = float4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
    aabb = aabb.xwzy * float4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f; // clip space -> uv space

    return true;
}

bool OcclusionCull(float3 center, float radius)
{
    center = mul(CameraCB.mtxView, float4(center, 1.0)).xyz;
    
    float4 aabb;
    if (ProjectSphere(center, radius, CameraCB.nearZ, CameraCB.mtxProjection[0][0], CameraCB.mtxProjection[1][1], aabb))
    {
        Texture2D<float> hzbTexture = ResourceDescriptorHeap[SceneCB.reprojectedHZBSRV];
        SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.minReductionSampler];
        
        float width = (aabb.z - aabb.x) * SceneCB.reprojectedHZBWidth;
        float height = (aabb.w - aabb.y) * SceneCB.reprojectedHZBHeight;
        float level = ceil(log2(max(width, height)));
        
        float depth = hzbTexture.SampleLevel(minReductionSampler, (aabb.xy + aabb.zw) * 0.5, level).x;
        float depthSphere = GetNdcDepth(center.z - radius);

        bool visible = depthSphere > depth;
        return !visible;
    }
    
    return false;
}

bool Cull(Meshlet meshlet)
{
    // 1. frustum culling
    float3 center = mul(ModelCB.mtxWorld, float4(meshlet.center, 1.0)).xyz;
    float radius = meshlet.radius * ModelCB.scale;
    
    for (uint i = 0; i < 6; ++i)
    {
        if (dot(center, CameraCB.culling.planes[i].xyz) + CameraCB.culling.planes[i].w + radius < 0)
        {
            stats(STATS_FRUSTUM_CULLED_MESHLET, 1);
            return false;
        }
    }
    
    // 2. backface culling
    int16_t4 cone = unpack_s8s16((int8_t4_packed)meshlet.cone);
    float3 axis = cone.xyz / 127.0;
    float cutoff = cone.w / 127.0;
    
    axis = normalize(mul(ModelCB.mtxWorld, float4(axis, 0.0)).xyz);
    float3 view = center - CameraCB.culling.viewPos;
    
    if (dot(view, -axis) >= cutoff * length(view) + radius)
    {
        stats(STATS_BACKFACE_CULLED_MESHLET, 1);
        return false;
    }
    
    // 3. occlusion culling
    if(OcclusionCull(center, radius))
    {
        stats(STATS_OCCLUSION_CULLED_MESHLET, 1);
        return false;
    }

    return true;
}

[numthreads(32, 1, 1)]
void main_as(uint dispatchThreadID : SV_DispatchThreadID)
{
    bool visible = false;
    uint meshletIndex = dispatchThreadID;
    
    if (meshletIndex < c_meshletCount)
    {
        StructuredBuffer<Meshlet> meshletBuffer = ResourceDescriptorHeap[c_meshletBuffer];
        Meshlet meshlet = meshletBuffer[meshletIndex];
        
        visible = Cull(meshlet);
        
        stats(visible ? STATS_RENDERED_TRIANGLE : STATS_CULLED_TRIANGLE, meshlet.triangleCount);
    }
    
    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.meshletIndices[index] = meshletIndex;
    }

    uint visibleMeshletCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleMeshletCount, 1, 1, s_Payload);
}

struct VertexOut
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    uint meshlet : COLOR;
};

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload Payload payload,
    out indices uint3 indices[124],
    out vertices VertexOut vertices[64])
{
    uint meshletIndex = payload.meshletIndices[groupID];
    if(meshletIndex >= c_meshletCount)
    {
        return;
    }
    
    StructuredBuffer<Meshlet> meshletBuffer = ResourceDescriptorHeap[c_meshletBuffer];
    Meshlet meshlet = meshletBuffer[meshletIndex];
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        StructuredBuffer<uint16_t> meshletIndicesBuffer = ResourceDescriptorHeap[c_meshletIndicesBuffer];
        uint3 index = uint3(
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 1],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 2]);
        
        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {
        StructuredBuffer<uint> meshletVerticesBuffer = ResourceDescriptorHeap[c_meshletVerticesBuffer];
        uint vertex_id = meshletVerticesBuffer[meshlet.vertexOffset + groupThreadID];
        
        StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
        StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
        
        float4 pos = float4(posBuffer[vertex_id], 1.0);
        float4 worldPos = mul(ModelCB.mtxWorld, pos);
        
        VertexOut vertex;
        vertex.pos = mul(CameraCB.mtxViewProjection, worldPos);
        vertex.normal = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(normalBuffer[vertex_id], 0.0f)).xyz);
        vertex.meshlet = meshletIndex;
        
        vertices[groupThreadID] = vertex;
    }
}


uint hash(uint a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

struct GBufferOutput
{
    float4 albedoRT : SV_TARGET0;
    float4 normalRT : SV_TARGET1;
    float4 emissiveRT : SV_TARGET2;
};

GBufferOutput main_ps(VertexOut input)
{
    uint mhash = hash(input.meshlet);
    float3 color = float3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
    
    GBufferOutput output;
    output.albedoRT = float4(color, 0);
    output.normalRT = float4(OctNormalEncode(input.normal), 1);
    output.emissiveRT = float4(0, 0, 0, 1);
    
    return output;
}