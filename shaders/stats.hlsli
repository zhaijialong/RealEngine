#pragma once

#include "global_constants.hlsli"

#define STATS_FRUSTUM_CULLED_MESHLET 0
#define STATS_BACKFACE_CULLED_MESHLET 1
#define STATS_OCCLUSION_CULLED_MESHLET 2
#define STATS_TYPE_COUNT 3


#ifndef __cplusplus

void stats(uint type, uint count)
{
    RWBuffer<uint> statsBuffer = ResourceDescriptorHeap[SceneCB.statsBufferUAV];
    
    InterlockedAdd(statsBuffer[type], count);
}

#endif