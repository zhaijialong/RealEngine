#include "physics_debug_renderer.h"
#include "../renderer/renderer.h"

PhysicsDebugRenderer::PhysicsDebugRenderer(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    Initialize();
}

void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
}

void PhysicsDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor)
{
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    return Batch();
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
{
    return Batch();
}

void PhysicsDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight)
{
}

void PhysicsDebugRenderer::Clear()
{
}

void PhysicsDebugRenderer::Draw(IGfxCommandList* pCommandList)
{
}
