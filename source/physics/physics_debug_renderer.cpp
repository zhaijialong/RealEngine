#include "physics_debug_renderer.h"
#include "../renderer/renderer.h"

class PhysicsBatch : public JPH::RefTargetVirtual
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

    PhysicsBatch(Renderer* pRenderer, const JPH::DebugRenderer::Triangle* inTriangles, int inTriangleCount)
    {
        m_pVertexBuffer.reset(pRenderer->CreateStructuredBuffer(inTriangles, sizeof(JPH::DebugRenderer::Vertex), inTriangleCount * 3, "PhysicsBatch vertices"));
    }

    PhysicsBatch(Renderer* pRenderer, const JPH::DebugRenderer::Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
    {
        m_pVertexBuffer.reset(pRenderer->CreateStructuredBuffer(inVertices, sizeof(JPH::DebugRenderer::Vertex), inVertexCount, "PhysicsBatch vertices"));
        m_pIndexBuffer.reset(pRenderer->CreateIndexBuffer(inIndices, sizeof(JPH::uint32), inIndexCount, "PhysicsBatch indices"));
    }

    StructuredBuffer* GetVertexBuffer() const { return m_pVertexBuffer.get(); }
    IndexBuffer* GetIndexBuffer() const { return m_pIndexBuffer.get(); }

private:
    uint32_t m_refCount = 0;

    eastl::unique_ptr<StructuredBuffer> m_pVertexBuffer;
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer;
};

PhysicsDebugRenderer::PhysicsDebugRenderer(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    Initialize();
}

void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    // todo
}

void PhysicsDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor)
{
    // todo
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    return new PhysicsBatch(m_pRenderer, inTriangles, inTriangleCount);
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
{
    return new PhysicsBatch(m_pRenderer, inVertices, inVertexCount, inIndices, inIndexCount);
}

void PhysicsDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight)
{
    // todo
}

void PhysicsDebugRenderer::Clear()
{
}

void PhysicsDebugRenderer::Draw()
{
}
