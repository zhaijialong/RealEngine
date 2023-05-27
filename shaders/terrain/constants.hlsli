#pragma once

#ifdef __cplusplus
struct ErosionConstants
#else
cbuffer CB1 : register(b1)
#endif
{
    uint c_heightmapUAV0;
    uint c_heightmapUAV1;
    uint c_sedimentUAV0;
    uint c_sedimentUAV1;
    
    uint c_waterUAV;
    uint c_fluxUAV;
    uint c_velocityUAV0;
    uint c_velocityUAV1;
    
    uint c_regolithUAV;
    uint c_regolithFluxUAV;
    uint c_bRain;
    float c_rainRate;
    
    float c_evaporationRate;
    float c_erosionConstant;
    float c_depositionConstant;
    float c_sedimentCapacityConstant;
    
    float c_smoothness;    
    float c_maxRegolith;
};

#ifndef __cplusplus
static const float density = 1.0;
static const float gravity = 9.8;
static const float pipeLength = 0.8;
static const float pipeArea = 2.0;
static const float deltaTime = 0.05;
static const float minTiltAngle = radians(1.0);

static const float2 terrainOrigin = float2(-20.0, -20.0);
static const float terrainSize = 40.0;
static const float terrainHeightScale = 30.0;
#endif