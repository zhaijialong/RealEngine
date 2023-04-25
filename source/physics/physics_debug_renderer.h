#pragma once

#include "Jolt/Jolt.h"
#include "Jolt/Renderer/DebugRenderer.h"

class IGfxCommandList;
class Renderer;

class PhysicsDebugRenderer : public JPH::DebugRenderer
{
public:
    PhysicsDebugRenderer(Renderer* pRenderer);

    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor) override;
    virtual Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
    virtual Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;
    virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;
    virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override;

    void Clear();
    void Draw(IGfxCommandList* pCommandList);

private:
    Renderer* m_pRenderer;
};