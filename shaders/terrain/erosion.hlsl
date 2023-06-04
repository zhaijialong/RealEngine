#include "../common.hlsli"
#include "../random.hlsli"
#include "constants.hlsli"
#include "noise.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_passIndex;
}

static RWTexture2D<float> heightmapUAV = ResourceDescriptorHeap[c_heightmapUAV];
static RWTexture2D<float> waterUAV = ResourceDescriptorHeap[c_waterUAV];
static RWTexture2D<float4> fluxUAV = ResourceDescriptorHeap[c_fluxUAV];
static RWTexture2D<float2> velocityUAV = ResourceDescriptorHeap[c_velocityUAV];
static RWTexture2D<float> sedimentUAV0 = ResourceDescriptorHeap[c_sedimentUAV0];
static RWTexture2D<float> sedimentUAV1 = ResourceDescriptorHeap[c_sedimentUAV1];
static RWTexture2D<uint4> soilFluxUAV = ResourceDescriptorHeap[c_soilFluxUAV];
static RWTexture2D<float> hardnessUAV = ResourceDescriptorHeap[c_hardnessUAV];

float GetHeight(int2 pos)
{
    uint width, height;
    heightmapUAV.GetDimensions(width, height);

    return heightmapUAV[clamp(pos, 0, int2(width - 1, height - 1))];
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

float GetHardness(int2 pos)
{
    return hardnessUAV[pos];
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
    uint width, height;
    waterUAV.GetDimensions(width, height);
    
    PRNG rng;
    rng.seed = pos.x + pos.y * width;
    
    float water = max(rng.RandomFloat(), 0.2);

    waterUAV[pos] += water * c_rainRate * deltaTime;
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

    float velocityFactor = pipeLength * (waterHeight + newWaterHeight) * 0.5;
    float2 velocity = (velocityFactor > 0.00005) ? float2(deltaX, deltaY) / velocityFactor : 0.0;
    
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
    
    float sediment = sedimentUAV0[pos];
    float2 velocity = velocityUAV[pos];
    float waterHeight = waterUAV[pos];
    
    const float maxErosionDepth = 0.0001; //todo
    float lmax = saturate(1 - (maxErosionDepth - waterHeight) / maxErosionDepth);
    float sedimentCapacity = length(velocity) * c_sedimentCapacityConstant * abs(sin(tiltAngle)) * lmax;

    if (sediment > sedimentCapacity)
    {
        float sedimentDiff = (sediment - sedimentCapacity) * c_depositionConstant;
        sedimentDiff = clamp(sedimentDiff, 0.0, waterHeight);

        heightmapUAV[pos] += sedimentDiff;
        sedimentUAV0[pos] -= sedimentDiff;
        //waterUAV[pos] -= sedimentDiff;
    }
    else
    {
        float sedimentDiff = (sedimentCapacity - sediment) * c_erosionConstant * GetHardness(pos);
        
        heightmapUAV[pos] -= sedimentDiff;
        sedimentUAV0[pos] += sedimentDiff;
        //waterUAV[pos] += sedimentDiff;
    }
    
    hardnessUAV[pos] = max(0.1, GetHardness(pos) - c_sedimentSofteningConstant * c_erosionConstant * (sediment - sedimentCapacity));

}

void advect_sediment(int2 pos)
{    
    float2 velocity = velocityUAV[pos];
    float previousX = pos.x - velocity.x * deltaTime;
    float previousY = pos.y - velocity.y * deltaTime;

    sedimentUAV1[pos] = BilinearSample(sedimentUAV0, float2(previousX, previousY));
}

struct SoilFlux
{
    float L;
    float R;
    float T;
    float B;
    float LT;
    float LB;
    float RT;
    float RB;
};

uint4 pack_soil_flux(SoilFlux flux)
{
    uint4 x;
    x.x = (f32tof16(flux.L) << 16) | f32tof16(flux.R);
    x.y = (f32tof16(flux.T) << 16) | f32tof16(flux.B);
    x.z = (f32tof16(flux.LT) << 16) | f32tof16(flux.LB);
    x.w = (f32tof16(flux.RT) << 16) | f32tof16(flux.RB);
    return x;
}

SoilFlux unpack_soil_flux(uint4 x)
{
    SoilFlux flux;
    flux.L = f16tof32(x.x >> 16);
    flux.R = f16tof32(x.x & 0xffff);
    flux.T = f16tof32(x.y >> 16);
    flux.B = f16tof32(x.y & 0xffff);
    flux.LT = f16tof32(x.z >> 16);
    flux.LB = f16tof32(x.z & 0xffff);
    flux.RT = f16tof32(x.w >> 16);
    flux.RB = f16tof32(x.w & 0xffff);
    return flux;
}

void thermal_flow(int2 pos)
{
    float height = GetHeight(pos);
    float heightL = GetHeight(pos + int2(-1, 0));
    float heightR = GetHeight(pos + int2(1, 0));
    float heightT = GetHeight(pos + int2(0, -1));
    float heightB = GetHeight(pos + int2(0, 1));
    float heightLT = GetHeight(pos + int2(-1, -1));
    float heightLB = GetHeight(pos + int2(-1, 1));
    float heightRT = GetHeight(pos + int2(1, -1));
    float heightRB = GetHeight(pos + int2(1, 1));
    
    float deltaL = max(0.0, height - heightL);
    float deltaR = max(0.0, height - heightR);
    float deltaT = max(0.0, height - heightT);
    float deltaB = max(0.0, height - heightB);
    float deltaLT = max(0.0, height - heightLT);
    float deltaLB = max(0.0, height - heightLB);
    float deltaRT = max(0.0, height - heightRT);
    float deltaRB = max(0.0, height - heightRB);
    
    uint w, h;
    heightmapUAV.GetDimensions(w, h);
    
    const float cellLength = terrainSize / w;
    const float cellArea = cellLength * cellLength;
    
    float maxHeightDiff = max(max(max(deltaL, deltaR), max(deltaT, deltaB)), max(max(deltaLT, deltaLB), max(deltaRT, deltaRB)));
    float volumeToMove = pipeArea * c_thermalErosionConstant * GetHardness(pos) * maxHeightDiff * 0.5;

    float angleThreshold = GetHardness(pos) * c_talusAngleTan + c_talusAngleBias;
    float sumHeightDiff = 0.0;
    
    if (deltaL * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaL;
    if (deltaR * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaR;
    if (deltaT * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaT;
    if (deltaB * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaB;
    if (deltaLT * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaLT;
    if (deltaLB * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaLB;
    if (deltaRT * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaRT;    
    if (deltaRB * terrainHeightScale / cellLength > angleThreshold) sumHeightDiff += deltaRB;

    SoilFlux flux = (SoilFlux)0;
    if (sumHeightDiff > 0)
    {
        flux.L = volumeToMove * deltaL / sumHeightDiff;
        flux.R = volumeToMove * deltaR / sumHeightDiff;
        flux.T = volumeToMove * deltaT / sumHeightDiff;
        flux.B = volumeToMove * deltaB / sumHeightDiff;
        flux.LT = volumeToMove * deltaLT / sumHeightDiff;
        flux.LB = volumeToMove * deltaLB / sumHeightDiff;
        flux.RT = volumeToMove * deltaRT / sumHeightDiff;
        flux.RB = volumeToMove * deltaRB / sumHeightDiff;
    }
    
    soilFluxUAV[pos] = pack_soil_flux(flux);
}

SoilFlux GetSoilFlux(int2 pos)
{
    int width, height;
    soilFluxUAV.GetDimensions(width, height);
    
    if (any(pos < 0) || any(pos >= int2(width, height)))
    {
        return (SoilFlux)0;
    }
    
    return unpack_soil_flux(soilFluxUAV[pos]);
}

void thermal_erosion(int2 pos)
{
    SoilFlux flux = GetSoilFlux(pos);
    SoilFlux fluxL = GetSoilFlux(pos + int2(-1, 0));
    SoilFlux fluxR = GetSoilFlux(pos + int2(1, 0));
    SoilFlux fluxT = GetSoilFlux(pos + int2(0, -1));
    SoilFlux fluxB = GetSoilFlux(pos + int2(0, 1));
    SoilFlux fluxLT = GetSoilFlux(pos + int2(-1, -1));
    SoilFlux fluxLB = GetSoilFlux(pos + int2(-1, 1));
    SoilFlux fluxRT = GetSoilFlux(pos + int2(1, -1));
    SoilFlux fluxRB = GetSoilFlux(pos + int2(1, 1));
    
    float flowIn = fluxL.R + fluxR.L + fluxT.B + fluxB.T + fluxLT.RB + fluxLB.RT + fluxRT.LB + fluxRB.LT;
    float flowOut = flux.L + flux.R + flux.T + flux.B + flux.LT + flux.LB + flux.RT + flux.RB;

    heightmapUAV[pos] += (flowIn - flowOut) * deltaTime / (pipeLength * pipeLength);
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
            thermal_flow(dispatchThreadID.xy);
            break;
        case 4:
            force_based_erosion(dispatchThreadID.xy);
            break;
        case 5:
            advect_sediment(dispatchThreadID.xy);
            break;
        case 6:
            thermal_erosion(dispatchThreadID.xy);
            break;
        case 7:
            evaporation(dispatchThreadID.xy);
            break;
        default:
            break;
    }
}
