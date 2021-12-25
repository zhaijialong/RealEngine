#pragma once

#include "global_constants.hlsli"

#define STATS_FRUSTUM_CULLED_MESHLET 0
#define STATS_BACKFACE_CULLED_MESHLET 1
#define STATS_OCCLUSION_CULLED_MESHLET 2
#define STATS_CULLED_TRIANGLE 3
#define STATS_RENDERED_TRIANGLE 4

#define STATS_MAX_TYPE_COUNT 32


#ifndef __cplusplus

void stats(uint type, uint count)
{
    if (type < STATS_MAX_TYPE_COUNT)
    {
        RWBuffer<uint> statsBuffer = ResourceDescriptorHeap[SceneCB.statsBufferUAV];
    
        InterlockedAdd(statsBuffer[type], count);
    }
}

#endif