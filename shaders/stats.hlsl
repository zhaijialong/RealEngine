#include "stats.hlsli"
#include "debug.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_statsBufferSRV;
};

[numthreads(1, 1, 1)]
void main()
{
    if (!SceneCB.bEnableStats)
    {
        return;
    }

    Buffer<uint> statsBuffer = ResourceDescriptorHeap[c_statsBufferSRV];

    float2 screenPos = float2(100, 100);
    float3 color = float3(1, 1, 1);
    
    debug::PrintString(screenPos, color, '1', 's', 't', ' ', 'p', 'h', 'a', 's', 'e');

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 'o', 'b', 'j', 'e', 'c', 't', 's', ':');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_CULLED_OBJECTS]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_RENDERED_OBJECTS]);

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 'm', 'e', 's', 'h', 'l', 'e', 't', 's', ':');
    debug::PrintString(screenPos, color, 'f', 'r', 'u', 's', 't', 'u', 'm', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_FRUSTUM_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'b', 'a', 'c', 'k', 'f', 'a', 'c', 'e', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_BACKFACE_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'o', 'c', 'c', 'l', 'u', 's', 'i', 'o', 'n', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_OCCLUSION_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_RENDERED_MESHLET]);

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 's', ':');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_CULLED_TRIANGLE]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_1ST_PHASE_RENDERED_TRIANGLE]);

    screenPos.x = 100;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, '2', 'n', 'd', ' ', 'p', 'h', 'a', 's', 'e');

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 'o', 'b', 'j', 'e', 'c', 't', 's', ':');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_CULLED_OBJECTS]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_RENDERED_OBJECTS]);

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 'm', 'e', 's', 'h', 'l', 'e', 't', 's', ':');
    debug::PrintString(screenPos, color, 'f', 'r', 'u', 's', 't', 'u', 'm', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_FRUSTUM_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'b', 'a', 'c', 'k', 'f', 'a', 'c', 'e', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_BACKFACE_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'o', 'c', 'c', 'l', 'u', 's', 'i', 'o', 'n', ' ');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_OCCLUSION_CULLED_MESHLET]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_RENDERED_MESHLET]);

    screenPos.x = 120;
    screenPos.y += 20;
    debug::PrintString(screenPos, color, 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 's', ':');
    debug::PrintString(screenPos, color, 'c', 'u', 'l', 'l', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_CULLED_TRIANGLE]);
    debug::PrintString(screenPos, color, ',', ' ');
    debug::PrintString(screenPos, color, 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ');
    debug::PrintInt(screenPos, color, statsBuffer[STATS_2ND_PHASE_RENDERED_TRIANGLE]);
}