#include "jolt_debug_renderer.h"
#include "../../renderer/renderer.h"
#include "physics_debug.hlsli"

#ifdef JPH_DEBUG_RENDERER

class JoltBatch : public JPH::RefTargetVirtual
{
public:
    virtual void AddRef() override 
    { 
        ++m_refCount;
    }

    virtual void Release() override 
    {
        if (--m_refCount == 0)
        {
            delete this;
        }
    }

    JoltBatch(Renderer* pRenderer, const JPH::DebugRenderer::Triangle* inTriangles, int inTriangleCount)
    {
        m_pVertexBuffer.reset(pRenderer->CreateStructuredBuffer(inTriangles, sizeof(JPH::DebugRenderer::Vertex), inTriangleCount * 3, "PhysicsBatch vertices"));
        m_vertexCount = inTriangleCount * 3;
    }

    JoltBatch(Renderer* pRenderer, const JPH::DebugRenderer::Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
    {
        m_pVertexBuffer.reset(pRenderer->CreateStructuredBuffer(inVertices, sizeof(JPH::DebugRenderer::Vertex), inVertexCount, "PhysicsBatch vertices"));
        m_pIndexBuffer.reset(pRenderer->CreateIndexBuffer(inIndices, sizeof(JPH::uint32), inIndexCount, "PhysicsBatch indices"));
        m_vertexCount = inVertexCount;
        m_indexCount = inIndexCount;
    }

    StructuredBuffer* GetVertexBuffer() const { return m_pVertexBuffer.get(); }
    IndexBuffer* GetIndexBuffer() const { return m_pIndexBuffer.get(); }
    uint32_t GetVertexCount() const { return m_vertexCount; }
    uint32_t GetIndexCount() const { return m_indexCount; }

private:
    uint32_t m_refCount = 0;

    eastl::unique_ptr<StructuredBuffer> m_pVertexBuffer;
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
};

JoltDebugRenderer::JoltDebugRenderer(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    Initialize();

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = m_pRenderer->GetShader("physics_debug.hlsl", "vs_main", "vs_6_6", {});
    psoDesc.ps = m_pRenderer->GetShader("physics_debug.hlsl", "ps_main", "ps_6_6", {});
    psoDesc.rasterizer_state.wireframe = false;
    psoDesc.rasterizer_state.front_ccw = false; // Jolt uses CW order
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA16F;
    psoDesc.depthstencil_format = GfxFormat::D32F;

    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    m_PSOs[(int)ECullMode::CullBackFace] = m_pRenderer->GetPipelineState(psoDesc, "Physics Debug PSO");

    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Front;
    m_PSOs[(int)ECullMode::CullFrontFace] = m_pRenderer->GetPipelineState(psoDesc, "Physics Debug PSO");

    psoDesc.rasterizer_state.cull_mode = GfxCullMode::None;
    m_PSOs[(int)ECullMode::Off] = m_pRenderer->GetPipelineState(psoDesc, "Physics Debug PSO");
}

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    // todo
}

void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
{
    // todo
}

JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    return new JoltBatch(m_pRenderer, inTriangles, inTriangleCount);
}

JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
{
    return new JoltBatch(m_pRenderer, inVertices, inVertexCount, inIndices, inIndexCount);
}

void JoltDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
    // todo : culling, LOD, instancing ...
    JoltBatch* physicsBatch = (JoltBatch*)inGeometry->mLODs[0].mTriangleBatch.GetPtr();

    Constants constants;
    constants.vertexBuffer = physicsBatch->GetVertexBuffer()->GetSRV()->GetHeapIndex();
    constants.color = inModelColor.GetUInt32();
    memcpy(&constants.mtxWorld, &inModelMatrix, sizeof(float4x4));
    constants.mtxWorldInverseTranspose = transpose(inverse(constants.mtxWorld));

    RenderBatch& batch = m_pRenderer->AddForwardPassBatch();
    batch.label = "Physics Debug Render";
    batch.SetPipelineState(m_PSOs[(int)inCullMode]);
    batch.SetConstantBuffer(1, &constants, sizeof(constants));

    if (physicsBatch->GetIndexBuffer() != nullptr)
    {
        batch.SetIndexBuffer(physicsBatch->GetIndexBuffer()->GetBuffer(), 0, physicsBatch->GetIndexBuffer()->GetFormat());
        batch.DrawIndexed(physicsBatch->GetIndexCount());
    }
    else
    {
        batch.Draw(physicsBatch->GetVertexCount());
    }
}

void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight)
{
    // todo
}

#endif //JPH_DEBUG_RENDERER