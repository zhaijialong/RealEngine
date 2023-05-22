#include "../common.hlsli"
#include "../random.hlsli"
#include "noise.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_passIndex;
}

cbuffer CB1 : register(b1)
{
    uint c_heightmapUAV;
    uint c_sedimentUAV;
    uint c_waterUAV;
    uint c_fluxUAV;

    uint c_velocityUAV;    
    uint c_bRain;
    float c_rainRate;
    float c_evaporationRate;
    
    float4 c_sedimentCapacity; //4 layers

    float c_erosionConstant;
    float c_depositionConstant;
}

static RWTexture2D<float4> heightmapUAV = ResourceDescriptorHeap[c_heightmapUAV];
static RWTexture2D<float> sedimentUAV = ResourceDescriptorHeap[c_sedimentUAV];
static RWTexture2D<float> waterUAV = ResourceDescriptorHeap[c_waterUAV];
static RWTexture2D<float4> fluxUAV = ResourceDescriptorHeap[c_fluxUAV];
static RWTexture2D<float2> velocityUAV = ResourceDescriptorHeap[c_velocityUAV];

static const float density = 1.0;
static const float gravity = 9.8;
static const float pipeLength = 1.0;
static const float deltaTime = 0.03;
static const float minTiltAngle = radians(5.0);

// must match with terrain.hlsl
static const float terrainSize = 40.0;
static const float terrainHeightScale = 30.0;

float GetHeight(int2 pos)
{
    uint width, height;
    heightmapUAV.GetDimensions(width, height);
    
    float4 heights = heightmapUAV[clamp(pos, 0, int2(width - 1, height - 1))];
    return dot(heights, 1.0);
}

float GetWaterHeight(int2 pos)
{
    uint width, height;
    waterUAV.GetDimensions(width, height);
    
    return waterUAV[clamp(pos, 0, int2(width - 1, height - 1))];
}

float4 GetWaterFlux(int2 pos)
{
    uint width, height;
    fluxUAV.GetDimensions(width, height);
    
    if (any(pos < 0) || any(pos >= int2(width, height)))
    {
        return 0.0;
    }
    
    return fluxUAV[pos];
}

uint GetTopmostLayer(float4 heights)
{
    if (heights[3] > 0)
    {
        return 3;
    }
    else if (heights[2] > 0)
    {
        return 2;
    }
    else if (heights[1] > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void rain(uint2 pos)
{
    if (c_bRain && (SceneCB.frameIndex % 10 == 0))
    {
        uint width, height;
        waterUAV.GetDimensions(width, height);
    
        PRNG rng = PRNG::Create(pos, uint2(width, height));
        float water = rng.RandomFloat();

        waterUAV[pos] += water * c_rainRate;
    }
}

void evaporation(uint2 pos)
{
    waterUAV[pos] = waterUAV[pos] * (1.0 - c_evaporationRate);
}

void flow(int2 pos)
{
    float height = GetHeight(pos);
    float heightL = GetHeight(pos + int2(-1, 0));
    float heightR = GetHeight(pos + int2(1, 0));
    float heightT = GetHeight(pos + int2(0, -1));
    float heightB = GetHeight(pos + int2(0, 1));

    float waterHeight = GetWaterHeight(pos);
    float waterHeightL = GetWaterHeight(pos + int2(-1, 0));
    float waterHeightR = GetWaterHeight(pos + int2(1, 0));
    float waterHeightT = GetWaterHeight(pos + int2(0, -1));
    float waterHeightB = GetWaterHeight(pos + int2(0, 1));
    
    float deltaL = height + waterHeight - heightL - waterHeightL;
    float deltaR = height + waterHeight - heightR - waterHeightR;
    float deltaT = height + waterHeight - heightT - waterHeightT;
    float deltaB = height + waterHeight - heightB - waterHeightB;
    
    float staticPressureL = density * gravity * deltaL;
    float staticPressureR = density * gravity * deltaR;
    float staticPressureT = density * gravity * deltaT;
    float staticPressureB = density * gravity * deltaB;
    
    float accelerationL = staticPressureL / (density * pipeLength);
    float accelerationR = staticPressureR / (density * pipeLength);
    float accelerationT = staticPressureT / (density * pipeLength);
    float accelerationB = staticPressureB / (density * pipeLength);
    
    float4 flux = fluxUAV[pos];
    float fluxL = max(0.0, flux.x + deltaTime * pipeLength * pipeLength * accelerationL);
    float fluxR = max(0.0, flux.y + deltaTime * pipeLength * pipeLength * accelerationR);
    float fluxT = max(0.0, flux.z + deltaTime * pipeLength * pipeLength * accelerationT);
    float fluxB = max(0.0, flux.w + deltaTime * pipeLength * pipeLength * accelerationB);
    
    float waterOutflow = deltaTime * (fluxL + fluxR + fluxT + fluxB);
    float K = min(1.0, waterHeight * pipeLength * pipeLength / waterOutflow);
    
    uint w, h;
    fluxUAV.GetDimensions(w, h);
    
    fluxL = pos.x == 0 ? 0 : fluxL;
    fluxR = pos.x == w - 1 ? 0 : fluxR;
    fluxT = pos.y == 0 ? 0 : fluxT;
    fluxB = pos.y == h - 1 ? 0 : fluxB;
    
    fluxUAV[pos] = float4(fluxL, fluxR, fluxT, fluxB) * K;
}

void update_water(int2 pos)
{
    float4 flux = GetWaterFlux(pos);
    float4 fluxL = GetWaterFlux(pos + int2(-1, 0));
    float4 fluxR = GetWaterFlux(pos + int2(1, 0));
    float4 fluxT = GetWaterFlux(pos + int2(0, -1));
    float4 fluxB = GetWaterFlux(pos + int2(0, 1));
    
    float flowIn = fluxL.y + fluxR.x + fluxT.w + fluxB.z;
    float flowOut = dot(flux, 1.0);

    float waterHeight = waterUAV[pos];
    float newWaterHeight = waterHeight + (flowIn - flowOut) * deltaTime / (pipeLength * pipeLength);
    
    float deltaX = (fluxL.y - flux.x + flux.y - fluxR.x) * 0.5;
    float deltaY = (fluxT.w - flux.z + flux.w - fluxB.z) * 0.5;
    
    float avgHeight = (waterHeight + newWaterHeight) * 0.5;
    float velocityFactor = pipeLength * avgHeight;
    float2 velocity = velocityFactor > 1e-6 ? float2(deltaX, deltaY) / velocityFactor : float2(0, 0);
    
    waterUAV[pos] = newWaterHeight;
    velocityUAV[pos] = velocity;
}

void force_based_erosion(int2 pos)
{
    float heightL = GetHeight(pos + int2(-1, 0));
    float heightR = GetHeight(pos + int2(1, 0));
    float heightT = GetHeight(pos + int2(0, -1));
    float heightB = GetHeight(pos + int2(0, 1));
    float3 normal = normalize(float3((heightL - heightR) * terrainHeightScale, (heightT - heightB) * terrainHeightScale, 2.0 / terrainSize));
    float tiltAngle = acos(dot(normal, float3(0, 0, 1)));
    tiltAngle = max(tiltAngle, minTiltAngle);
    
    float4 heights = heightmapUAV[pos];
    float sediment = sedimentUAV[pos];
    float2 velocity = velocityUAV[pos];
    
    float4 sedimentCapacity = length(velocity) * c_sedimentCapacity * abs(sin(tiltAngle));
    
    uint topmostLayer = GetTopmostLayer(heights);
    if (sediment > sedimentCapacity[topmostLayer])
    {
        float sedimentDiff = (sediment - sedimentCapacity[topmostLayer]) * c_depositionConstant;
        heightmapUAV[pos][topmostLayer] += sedimentDiff;
        sedimentUAV[pos] -= sedimentDiff;
    }
    else
    {
        for (int k = 3; k > 0; --k)
        {
            if (k > topmostLayer)
            {
                heightmapUAV[pos][k] = 0.0;
            }

            //todo
        }
    }
}

void advect_sediment(int2 pos)
{
    //todo
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    switch (c_passIndex)
    {
        case 0:
            rain(dispatchThreadID.xy);
            break;
        case 1:
            flow(dispatchThreadID.xy);
            break;
        case 2:
            update_water(dispatchThreadID.xy); //todo : velocity diffuse
            break;
        case 3:
            force_based_erosion(dispatchThreadID.xy);
            break;
        case 4:
            advect_sediment(dispatchThreadID.xy);
            break;
        case 5:
            evaporation(dispatchThreadID.xy);
            break;
        default:
            break;
    }
}
