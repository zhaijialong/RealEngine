#include "../common.hlsli"
#include "../random.hlsli"
#include "constants.hlsli"
#include "noise.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_passIndex;
}

static RWTexture2D<float4> heightmapUAV = ResourceDescriptorHeap[c_heightmapUAV0];
static RWTexture2D<float> waterUAV = ResourceDescriptorHeap[c_waterUAV];
static RWTexture2D<float4> fluxUAV = ResourceDescriptorHeap[c_fluxUAV];
static RWTexture2D<float2> velocityUAV0 = ResourceDescriptorHeap[c_velocityUAV0];
static RWTexture2D<float2> velocityUAV1 = ResourceDescriptorHeap[c_velocityUAV1];
static RWTexture2D<float> sedimentUAV0 = ResourceDescriptorHeap[c_sedimentUAV0];
static RWTexture2D<float> sedimentUAV1 = ResourceDescriptorHeap[c_sedimentUAV1];

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

template<typename T>
T BilinearSample(RWTexture2D<T> sampledTexture, float2 pos)
{
    T s00 = sampledTexture[uint2(floor(pos.x), floor(pos.y))];
    T s10 = sampledTexture[uint2(ceil(pos.x), floor(pos.y))];
    T s01 = sampledTexture[uint2(floor(pos.x), ceil(pos.y))];
    T s11 = sampledTexture[uint2(ceil(pos.x), ceil(pos.y))];
    return lerp(lerp(s00, s10, frac(pos.x)), lerp(s01, s11, frac(pos.x)), frac(pos.y));
}

void rain(uint2 pos)
{
    if (c_bRain)
    {
        uint width, height;
        waterUAV.GetDimensions(width, height);
    
        PRNG rng = PRNG::Create(pos, uint2(width, height));
        float water = max(rng.RandomFloat(), 0.2);

        waterUAV[pos] += water * c_rainRate * deltaTime;
    }
}

void evaporation(uint2 pos)
{
    waterUAV[pos] = waterUAV[pos] * (1.0 - c_evaporationRate * deltaTime);
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
    float fluxL = max(0.0, flux.x + deltaTime * pipeArea * accelerationL);
    float fluxR = max(0.0, flux.y + deltaTime * pipeArea * accelerationR);
    float fluxT = max(0.0, flux.z + deltaTime * pipeArea * accelerationT);
    float fluxB = max(0.0, flux.w + deltaTime * pipeArea * accelerationB);
    
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
    float2 velocity = float2(deltaX, deltaY) / (pipeLength * avgHeight);
    
    if(newWaterHeight < 1e-4)
    {
        velocity = 0.0;
    }
    
    waterUAV[pos] = newWaterHeight;
    velocityUAV0[pos] = velocity;
}

// velocityUAV0 -> velocityUAV1
void diffuse_velocity0(int2 pos)
{
    uint width, height;
    velocityUAV0.GetDimensions(width, height);
    
    float2 velocity = velocityUAV0[pos];
    float2 velocityL = velocityUAV0[clamp(pos + int2(-1, 0), 0, int2(width - 1, height - 1))];
    float2 velocityR = velocityUAV0[clamp(pos + int2(1, 0), 0, int2(width - 1, height - 1))];
    float2 velocityT = velocityUAV0[clamp(pos + int2(0, -1), 0, int2(width - 1, height - 1))];
    float2 velocityB = velocityUAV0[clamp(pos + int2(0, 1), 0, int2(width - 1, height - 1))];
    
    velocityUAV1[pos] = (velocityL + velocityR + velocityT + velocityB + 4.0 * velocity) / 8.0;
}

// velocityUAV1 -> velocityUAV0
void diffuse_velocity1(int2 pos)
{
    uint width, height;
    velocityUAV0.GetDimensions(width, height);
    
    float2 velocity = velocityUAV1[pos];
    float2 velocityL = velocityUAV1[clamp(pos + int2(-1, 0), 0, int2(width - 1, height - 1))];
    float2 velocityR = velocityUAV1[clamp(pos + int2(1, 0), 0, int2(width - 1, height - 1))];
    float2 velocityT = velocityUAV1[clamp(pos + int2(0, -1), 0, int2(width - 1, height - 1))];
    float2 velocityB = velocityUAV1[clamp(pos + int2(0, 1), 0, int2(width - 1, height - 1))];
    
    velocityUAV0[pos] = (velocityL + velocityR + velocityT + velocityB + 4.0 * velocity) / 8.0;
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
    float sediment = sedimentUAV0[pos];
    float2 velocity = velocityUAV0[pos];
    
    float sedimentCapacity = length(velocity) * c_sedimentCapacityConstant * abs(sin(tiltAngle));
    uint topmostLayer = GetTopmostLayer(heights);

    if (sediment > sedimentCapacity)
    {
        float sedimentDiff = (sediment - sedimentCapacity) * c_depositionConstant;
        heightmapUAV[pos][0] += sedimentDiff;
        sedimentUAV0[pos] -= sedimentDiff;
    }
    else
    {
        float sedimentDiff = (sedimentCapacity - sediment) * c_erosionConstant[0];
        heightmapUAV[pos][0] -= sedimentDiff;
        sedimentUAV0[pos] += sedimentDiff;
        /*
        float layersHeight = 0.0;
        
        for (int k = topmostLayer; k > 0; --k)
        {
            float maxSediment = sedimentCapacity - layersHeight;
							
            if (maxSediment < sediment)
            {
                break;
            }

            layersHeight += heights[k];
            
            float sedimentDiff = (maxSediment - sediment) * c_erosionConstant[k];
            sedimentDiff = min(heights[k], sedimentDiff);
							
            sedimentUAV[pos] += sedimentDiff;
            heightmapUAV[pos][k] -= sedimentDiff;
        }*/
    }
}

void advect_sediment(int2 pos)
{    
    float2 velocity = velocityUAV0[pos];
    float previousX = pos.x - velocity.x * deltaTime;
    float previousY = pos.y - velocity.y * deltaTime;

    sedimentUAV1[pos] = BilinearSample(sedimentUAV0, float2(previousX, previousY));
}

void smooth_height(int2 pos)
{    
    float4 C = heightmapUAV[pos];
    float sediment = sedimentUAV1[pos];
    if (sediment > 0.0)
    {
        uint width, height;
        heightmapUAV.GetDimensions(width, height);
    
        float4 T = heightmapUAV[clamp(pos + int2(0, -1), 0, int2(width - 1, height - 1))];
        float4 TR = heightmapUAV[clamp(pos + int2(1, -1), 0, int2(width - 1, height - 1))];
        float4 R = heightmapUAV[clamp(pos + int2(1, 0), 0, int2(width - 1, height - 1))];
        float4 BR = heightmapUAV[clamp(pos + int2(1, 1), 0, int2(width - 1, height - 1))];
        float4 B = heightmapUAV[clamp(pos + int2(0, 1), 0, int2(width - 1, height - 1))];
        float4 BL = heightmapUAV[clamp(pos + int2(-1, 1), 0, int2(width - 1, height - 1))];
        float4 L = heightmapUAV[clamp(pos + int2(-1, 0), 0, int2(width - 1, height - 1))];
        float4 TL = heightmapUAV[clamp(pos + int2(-1, -1), 0, int2(width - 1, height - 1))];
    
        float deltaT = dot(C - T, 1.0);
        float deltaTR = dot(C - TR, 1.0);
        float deltaR = dot(C - R, 1.0);
        float deltaBR = dot(C - BR, 1.0);
        float deltaB = dot(C - B, 1.0);
        float deltaBL = dot(C - BL, 1.0);
        float deltaL = dot(C - L, 1.0);
        float deltaTL = dot(C - TL, 1.0);
    
        float averageHeightDiff = abs((deltaL + deltaR + deltaT + deltaB + deltaTL + deltaTR + deltaBL + deltaBR) / 8.0);
    
        float threathhold = lerp(10.0, 200.0, 1.0 - c_smoothness) / 65536.0;
    
        if (((abs(deltaR) > threathhold && abs(deltaL) > threathhold) && deltaR * deltaL > 0.0) ||
            ((abs(deltaT) > threathhold && abs(deltaB) > threathhold) && deltaT * deltaB > 0.0) ||
            ((abs(deltaTR) > threathhold && abs(deltaBL) > threathhold) && deltaTR * deltaBL > 0.0) ||
            ((abs(deltaTL) > threathhold && abs(deltaBR) > threathhold) && deltaTL * deltaBR > 0.0))
        {
            float curWeight = 5.0;
            C = (C * curWeight + L + R + T + B + TL + TR + BL + BR) / (curWeight + 8.0);
        }
    } 
    
    RWTexture2D<float4> output = ResourceDescriptorHeap[c_heightmapUAV1];
    output[pos] = C;
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
            update_water(dispatchThreadID.xy);
            break;
        case 3:
            diffuse_velocity0(dispatchThreadID.xy);
            break;
        case 4:
            diffuse_velocity1(dispatchThreadID.xy);
            break;
        case 5:
            force_based_erosion(dispatchThreadID.xy);
            break;
        case 6:
            advect_sediment(dispatchThreadID.xy);
            break;
        case 7:
            evaporation(dispatchThreadID.xy);
            break;
        case 8:
            smooth_height(dispatchThreadID.xy);
            break;
        default:
            break;
    }
}
