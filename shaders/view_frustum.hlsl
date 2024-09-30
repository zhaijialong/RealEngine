#include "common.hlsli"
#include "debug.hlsli"

// the intersection point of three planes 
float3 IntersectPlanes(float4 p0, float4 p1, float4 p2)
{
    float3 n12 = cross(p1.xyz, p2.xyz);
    float3 n20 = cross(p2.xyz, p0.xyz);
    float3 n01 = cross(p0.xyz, p1.xyz);

    float3 r = -p0.w * n12 - p1.w * n20 - p2.w * n01;
    return r * (1 / dot(p0.xyz, n12));
}

[numthreads(1, 1, 1)]
void main()
{
    const float4 left = GetCameraCB().culling.planes[0];
    const float4 right = GetCameraCB().culling.planes[1];
    const float4 top = GetCameraCB().culling.planes[2];
    const float4 bottom = GetCameraCB().culling.planes[3];
    const float4 near = GetCameraCB().culling.planes[5];
    
    const float3 leftTop = IntersectPlanes(left, top, near);
    const float3 rightTop = IntersectPlanes(right, top, near);
    const float3 rightBottom = IntersectPlanes(right, bottom, near);
    const float3 leftBottom = IntersectPlanes(left, bottom, near);
    
    const float length = 1000000.0;
    const float3 color = float3(1, 0, 0);
    
    debug::DrawLine(leftTop, rightTop, color);
    debug::DrawLine(rightTop, rightBottom, color);
    debug::DrawLine(rightBottom, leftBottom, color);
    debug::DrawLine(leftBottom, leftTop, color);
    
    const float3 leftTopDir = cross(top.xyz, left.xyz);
    const float3 rightTopDir = cross(right.xyz, top.xyz);
    const float3 rightBottomDir = cross(bottom.xyz, right.xyz);
    const float3 leftBottomDir = cross(left.xyz, bottom.xyz);
    
    debug::DrawLine(leftTop, leftTop + leftTopDir * length, color);
    debug::DrawLine(rightTop, rightTop + rightTopDir * length, color);
    debug::DrawLine(rightBottom, rightBottom + rightBottomDir * length, color);
    debug::DrawLine(leftBottom, leftBottom + leftBottomDir * length, color);
}