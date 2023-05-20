#include "../common.hlsli"
#include "../atmosphere.hlsli"

cbuffer CB : register(b0)
{
    uint c_heightTexture;
    uint c_outputTexture;
}

static const float2 terrainOrigin = float2(-20.0, -20.0);
static const float terrainSize = 40.0;
static const float terrainHeightScale = 30.0;

float GetHeight(float x, float z)
{    
    float2 uv = (float2(x, z) - terrainOrigin) / terrainSize;
    if(any(uv < 0.0) || any(uv > 1.0))
    {
        return -10000000000.0;
    }    
    
    Texture2D heightTexture = ResourceDescriptorHeap[c_heightTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    uv.y = 1.0 - uv.y;
    return heightTexture.SampleLevel(linearSampler, uv, 0).x * terrainHeightScale;
}

float3 GetNormal(float3 p)
{
    const float eps = 0.5 / terrainSize;
    return normalize(float3(GetHeight(p.x - eps, p.z) - GetHeight(p.x + eps, p.z),
                            2.0f * eps,
                            GetHeight(p.x, p.z - eps) - GetHeight(p.x, p.z + eps)));
}

struct Ray
{
    float3 origin;
    float3 direction;
    float minT;
    float maxT;
};

// https://iquilezles.org/articles/terrainmarching/
bool CastRay(Ray ray, out float hitT)
{
    float dt = 0.01f;
    float lh = 0.0f;
    float ly = 0.0f;
    
    for (float t = ray.minT; t < ray.maxT; t += dt)
    {
        const float3 p = ray.origin + ray.direction * t;
        const float h = GetHeight(p.x, p.z);
        
        if (p.y < h)
        {
            // interpolate the intersection distance
            hitT = t - dt + dt * (lh - ly) / (p.y - ly - h + lh);
            return true;
        }
        
        // allow the error to be proportinal to the distance
        dt = 0.01f * t;
        lh = h;
        ly = p.y;
    }
    return false;
}

float3 TerrainColor(float3 position)
{
    float3 diffuseColor = saturate(GetHeight(position.x, position.z).xxx / 10.0 + 0.1);
    float3 N = GetNormal(position);
    float3 L = SceneCB.lightDir;
    
    return diffuseColor * (max(0, dot(N, L)) + DiffuseIBL(N));
}

float3 SkyColor(Ray ray)
{
    float rayLength = INFINITY;
    float3 lightDir = SceneCB.lightDir;
    float3 lightColor = SceneCB.lightColor;

    float3 transmittance;
    float3 color = IntegrateScattering(ray.origin, ray.direction, rayLength, lightDir, lightColor, 32, transmittance);
    return color;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    float3 position = GetWorldPosition(dispatchThreadID.xy, 0);
    
    Ray ray;
    ray.origin = GetCameraCB().cameraPos;
    ray.direction = normalize(position - GetCameraCB().cameraPos);
    ray.minT = 0.001f;
    ray.maxT = 100.0f;
    
    float hitT;
    if(CastRay(ray, hitT))
    {
        float3 position = ray.origin + ray.direction * hitT;
        
        outputTexture[dispatchThreadID.xy] = float4(TerrainColor(position), 1.0);
    }
    else
    {
        outputTexture[dispatchThreadID.xy] = float4(SkyColor(ray), 1.0);
    }
}