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

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count)
{
    JPH::TriangleList triangles;

    uint32_t triangle_count = vertex_count / 3;
    triangles.reserve(triangle_count);

    for (uint32_t i = 0; i < triangle_count; ++i)
    {
        JPH::Triangle triangle;
        memcpy(&triangle.mV[0], (const char*)vertices + (i * 3 + 0) * vertex_stride, sizeof(float3));
        memcpy(&triangle.mV[1], (const char*)vertices + (i * 3 + 1) * vertex_stride, sizeof(float3));
        memcpy(&triangle.mV[2], (const char*)vertices + (i * 3 + 2) * vertex_stride, sizeof(float3));

        triangles.push_back(triangle);
    }

    JPH::MeshShapeSettings settings(triangles);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}

template <typename T>
void ToJoltTriangles(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const T* indices, uint32_t index_count,
    JPH::VertexList& vertexList, JPH::IndexedTriangleList& triangleList)
{
    vertexList.reserve(vertex_count);

    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        JPH::Float3 v;
        memcpy(&v, (const char*)vertices + i * vertex_stride, sizeof(float3));

        vertexList.push_back(v);
    }

    uint32_t triangle_count = index_count / 3;
    triangleList.reserve(triangle_count);

    for (uint32_t i = 0; i < triangle_count; ++i)
    {
        JPH::IndexedTriangle triangle;
        triangle.mIdx[0] = (JPH::uint32)indices[i * 3 + 0];
        triangle.mIdx[1] = (JPH::uint32)indices[i * 3 + 1];
        triangle.mIdx[2] = (JPH::uint32)indices[i * 3 + 2];

        triangleList.push_back(triangle);
    }
}

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint16_t* indices, uint32_t index_count)
{
    JPH::VertexList vertexList;
    JPH::IndexedTriangleList triangleList;
    ToJoltTriangles(vertices, vertex_stride, vertex_count, indices, index_count, vertexList, triangleList);

    JPH::MeshShapeSettings settings(vertexList, triangleList);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}

bool JoltShape::CreateMesh(const float* vertices, uint32_t vertex_stride, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    JPH::VertexList vertexList;
    JPH::IndexedTriangleList triangleList;
    ToJoltTriangles(vertices, vertex_stride, vertex_count, indices, index_count, vertexList, triangleList);

    JPH::MeshShapeSettings settings(vertexList, triangleList);
    JPH::Shape::ShapeResult result;
    m_shape = new JPH::MeshShape(settings, result);

    return result.IsValid();
}
