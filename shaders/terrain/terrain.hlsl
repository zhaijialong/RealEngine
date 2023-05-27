#include "../common.hlsli"
#include "../debug.hlsli"
#include "../atmosphere.hlsli"
#include "../brdf.hlsli"
#include "constants.hlsli"

cbuffer CB : register(b0)
{
    uint c_heightTexture;
    uint c_waterTexture;
    uint c_waterFluxTexture;
    uint c_waterVelocityTexture;
    uint c_sedimentTexture;
    uint c_regolithTexture;
    uint c_regolithFluxTexture;
    uint c_outputTexture;
}


static Texture2D heightTexture = ResourceDescriptorHeap[c_heightTexture];
static Texture2D waterTexture = ResourceDescriptorHeap[c_waterTexture];
static Texture2D waterFluxTexture = ResourceDescriptorHeap[c_waterFluxTexture];
static Texture2D waterVelocityTexture = ResourceDescriptorHeap[c_waterVelocityTexture];
static Texture2D sedimentTexture = ResourceDescriptorHeap[c_sedimentTexture];
static Texture2D regolithTexture = ResourceDescriptorHeap[c_regolithTexture];
static Texture2D regolithFluxTexture = ResourceDescriptorHeap[c_regolithFluxTexture];
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

float2 GetUV(float x, float z)
{
    float2 uv = (float2(x, z) - terrainOrigin) / terrainSize;
    uv.y = 1.0 - uv.y;
    return uv;
}

float GetTerrainHeight(float x, float z)
{
    float2 uv = GetUV(x, z);
    if (any(uv < 0.0) || any(uv > 1.0))
    {
        return -10000000000.0;
    }

    return heightTexture.SampleLevel(linearSampler, uv, 0).x;
}

float GetWaterHeight(float x, float z)
{
    float2 uv = GetUV(x, z);
    if (any(uv < 0.0) || any(uv > 1.0))
    {
        return -10000000000.0;
    }

    return waterTexture.SampleLevel(linearSampler, uv, 0).x;    
}

float GetHeight(float x, float z)
{    
    float terrainHeight = GetTerrainHeight(x, z);
    float waterHeight = GetWaterHeight(x, z);
    
    return (terrainHeight + waterHeight) * terrainHeightScale;
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

float3 GetDiffuseColor(float x, float z)
{
    float3 diffuseColor = 0;    
    float height = GetTerrainHeight(x, z);

    if (height < 0.05)
    {
        diffuseColor = float3(0.05, 0.6, 0.05);
    }
    else if (height < 0.1)
    {
        diffuseColor = lerp(float3(0.05, 0.6, 0.05), float3(0.8, 0.5, 0.2), (height - 0.05) / 0.05);

    }
    else if (height < 0.25)
    {
        diffuseColor = float3(0.8, 0.5, 0.2);
    }
    else if (height < 0.3)
    {
        diffuseColor = lerp(float3(0.8, 0.5, 0.2), 1.0, (height - 0.25) / 0.05);
    }
    else
    {
        diffuseColor = 1.0;
    }

    return diffuseColor;
}

float3 TerrainColor(float3 position, float3 V)
{
    float2 uv = GetUV(position.x, position.z);
    //return saturate(waterVelocityTexture.SampleLevel(linearSampler, uv, 0).xyz * 0.02);
    //return saturate(waterFluxTexture.SampleLevel(linearSampler, uv, 0).xyz * 5.0);
    //return saturate(sedimentTexture.SampleLevel(linearSampler, uv, 0).xyz * 1000.0);
    
    float waterHeight = GetWaterHeight(position.x, position.z);
    float3 N = GetNormal(position);
    float3 terrainColor = GetDiffuseColor(position.x, position.z) * DiffuseIBL(N);
    
    if (waterHeight > 0)
    {
        float3 H = normalize(N + SceneCB.lightDir);
        
        float specular = pow(max(dot(N, H), 0.0), 333.0);
        float3 relectionColor = lerp(float3(0.6, 0.6, 0.6), float3(0.3, 0.5, 0.9), clamp(H.z, 0.f, 1.f));
        float R = saturate(0.2 + 0.2 * pow(1.0 + dot(V, -N), 22.0));
    
        float3 waterColor = float3(0.0, 0.3, 0.5) + relectionColor * R + specular;
        float alpha = min((1.8 + specular) * waterHeight * 20.0, 1.0);
        
        return lerp(terrainColor, waterColor, alpha);
    }

    return terrainColor;
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

void DrawRect(uint2 pos, uint4 rect, Texture2D texture, float scale = 1.0)
{
    if (pos.x >= rect.x && pos.x < rect.z && pos.y >= rect.y && pos.y < rect.w)
    {
        float2 uv = (pos - rect.xy + 0.5) / float2(rect.zw - rect.xy);
        float4 value = texture.SampleLevel(linearSampler, uv, 0) * scale;
        outputTexture[pos] = value;
    }
}

void DebugUI(uint2 pos)
{
    uint width, height;
    outputTexture.GetDimensions(width, height);
    
    if (all(pos == 0))
    {
        float2 screenPos = float2(0.0, 95.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 'w', 'a', 't', 'e', 'r');
        screenPos = float2(0.0, 315.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 'f', 'l', 'u', 'x');
        screenPos = float2(0.0, 535.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 'v', 'e', 'l', 'o', 'c', 'i', 't', 'y');
        screenPos = float2(0.0, 755.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 's', 'e', 'd', 'i', 'm', 'e', 'n', 't');
                
        screenPos = float2(width - 200, 95.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 'r', 'e', 'g', 'o', 'l', 'i', 't', 'h');
        screenPos = float2(width - 200, 315.0);
        debug::PrintString(screenPos, float3(1, 1, 1), 'r', 'e', 'g', 'o', 'l', 'i', 't', 'h');
        debug::PrintString(screenPos, float3(1, 1, 1), ' ', 'f', 'l', 'u', 'x');
    }
    
    DrawRect(pos, uint4(0, 100, 200, 300), waterTexture, 100.0);
    DrawRect(pos, uint4(0, 320, 200, 520), waterFluxTexture, 50.0);
    DrawRect(pos, uint4(0, 540, 200, 740), waterVelocityTexture, 0.02);
    DrawRect(pos, uint4(0, 760, 200, 960), sedimentTexture, 1000.0);
    
    DrawRect(pos, uint4(width - 200, 100, width, 300), regolithTexture, 1000.0);
    DrawRect(pos, uint4(width - 200, 320, width, 520), regolithFluxTexture, 500.0);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
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
        
        outputTexture[dispatchThreadID.xy] = float4(TerrainColor(position, -ray.direction), 1.0);
    }
    else
    {
        outputTexture[dispatchThreadID.xy] = float4(SkyColor(ray), 1.0);
    }
    
    DebugUI(dispatchThreadID.xy);
}
