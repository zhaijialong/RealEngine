#pragma once

#ifdef JPH_DEBUG_RENDERER

#include "Jolt/Jolt.h"
#include "Jolt/Renderer/DebugRenderer.h"

class Renderer;
class IGfxPipelineState;

class JoltDebugRenderer : public JPH::DebugRenderer
{
public:
    JoltDebugRenderer(Renderer* pRenderer);

    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;
    virtual Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
    virtual Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;
    virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;
    virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override;

private:
    Renderer* m_pRenderer;

    IGfxPipelineState* m_PSOs[3];
};

#endif //JPH_DEBUG_RENDERER