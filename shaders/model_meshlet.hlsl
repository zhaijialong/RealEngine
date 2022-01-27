#include "model.hlsli"
#include "meshlet.hlsli"

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload MeshletPayload payload,
    out indices uint3 indices[124],
    out vertices model::VertexOut vertices[64])
{
    uint instanceIndex = payload.instanceIndices[groupID];
    uint meshletIndex = payload.meshletIndices[groupID];
    if(meshletIndex >= GetInstanceData(instanceIndex).meshletCount)
    {
        return;
    }
    
    Meshlet meshlet = LoadSceneBuffer<Meshlet>(GetInstanceData(instanceIndex).meshletBufferAddress, meshletIndex);
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        uint3 index = uint3(
            LoadSceneBuffer<uint16_t>(GetInstanceData(instanceIndex).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3),
            LoadSceneBuffer<uint16_t>(GetInstanceData(instanceIndex).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 1),
            LoadSceneBuffer<uint16_t>(GetInstanceData(instanceIndex).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 2));

        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {
        uint vertex_id = LoadSceneBuffer<uint>(GetInstanceData(instanceIndex).meshletVerticesBufferAddress, meshlet.vertexOffset + groupThreadID);

        model::VertexOut v = model::GetVertex(instanceIndex, vertex_id);
        v.meshlet = meshletIndex;
        v.instanceIndex = instanceIndex;
        
        vertices[groupThreadID] = v;
    }
}
