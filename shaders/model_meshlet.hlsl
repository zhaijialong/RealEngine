#include "model.hlsli"
#include "meshlet.hlsli"

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload MeshletPayload payload,
    out indices uint3 indices[124],
    out vertices model::VertexOutput vertices[64])
{
    uint instanceIndex = payload.instanceIndices[groupID];
    uint meshletIndex = payload.meshletIndices[groupID];

    InstanceData instanceData = GetInstanceData(instanceIndex);
    if (meshletIndex >= instanceData.meshletCount)
    {
        return;
    }
    
    Meshlet meshlet = LoadSceneStaticBuffer<Meshlet>(instanceData.meshletBufferAddress, meshletIndex);
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        uint3 index = uint3(
            LoadSceneStaticBuffer<uint16_t>(instanceData.meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3),
            LoadSceneStaticBuffer<uint16_t>(instanceData.meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 1),
            LoadSceneStaticBuffer<uint16_t>(instanceData.meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 2));

        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {
        uint vertex_id = LoadSceneStaticBuffer<uint>(instanceData.meshletVerticesBufferAddress, meshlet.vertexOffset + groupThreadID);

        model::VertexOutput v = model::GetVertexOutput(instanceIndex, vertex_id);
        v.meshletIndex = meshletIndex;
        v.instanceIndex = instanceIndex;
        
        vertices[groupThreadID] = v;
    }
}
