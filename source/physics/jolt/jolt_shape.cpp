#include "jolt_shape.h"
#include "jolt_utils.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"

bool JoltShape::CreateBox(const float3& half_extent)
{
    JPH::BoxShapeSettings settings(ToJolt(half_extent));
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::BoxShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateSphere(float radius)
{
    JPH::SphereShapeSettings settings(radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::SphereShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateCapsule(float half_height, float radius)
{
    JPH::CapsuleShapeSettings settings(half_height, radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::CapsuleShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateCylinder(float half_height, float radius)
{
    JPH::CylinderShapeSettings settings(half_height, radius);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::CylinderShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateConvexHull(eastl::span<float3> points)
{
    JPH::Array<JPH::Vec3> joltPoints;
    joltPoints.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i)
    {
        joltPoints.push_back(ToJolt(points[i]));
    }

    JPH::ConvexHullShapeSettings settings(joltPoints);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::ConvexHullShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, bool winding_order_ccw)
{
    JPH::TriangleList triangles;

    uint32_t triangle_count = vertex_count / 3;
    triangles.reserve(triangle_count);

    for (uint32_t i = 0; i < triangle_count; ++i)
    {
        JPH::Triangle triangle;
        if (winding_order_ccw) // Jolt needs CW
        {
            memcpy(&triangle.mV[0], (const char*)vertices + (i * 3 + 0) * vertex_stride, sizeof(float3));
            memcpy(&triangle.mV[2], (const char*)vertices + (i * 3 + 1) * vertex_stride, sizeof(float3));
            memcpy(&triangle.mV[1], (const char*)vertices + (i * 3 + 2) * vertex_stride, sizeof(float3));
        }
        else
        {
            memcpy(&triangle.mV[0], (const char*)vertices + (i * 3 + 0) * vertex_stride, sizeof(float3));
            memcpy(&triangle.mV[1], (const char*)vertices + (i * 3 + 1) * vertex_stride, sizeof(float3));
            memcpy(&triangle.mV[2], (const char*)vertices + (i * 3 + 2) * vertex_stride, sizeof(float3));
        }

        triangles.push_back(triangle);
    }

    JPH::MeshShapeSettings settings(triangles);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count, bool winding_order_ccw)
{
    JPH::VertexList vertexList;
    JPH::IndexedTriangleList triangleList;
    ToJoltTriangles(vertices, vertex_stride, vertex_count, indices, index_count, vertexList, triangleList, winding_order_ccw);

    JPH::MeshShapeSettings settings(vertexList, triangleList);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, bool winding_order_ccw)
{
    JPH::VertexList vertexList;
    JPH::IndexedTriangleList triangleList;
    ToJoltTriangles(vertices, vertex_stride, vertex_count, indices, index_count, vertexList, triangleList, winding_order_ccw);

    JPH::MeshShapeSettings settings(vertexList, triangleList);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}
