#include "hsr_common.hlsli"
#include "ffx-reflection-dnsr/ffx_denoiser_reflections_common.h"

cbuffer Constants : register(b1)
{
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_samplesPerQuad;
    uint c_bEnableVarianceGuidedTracing;
    uint c_historyVarianceTexture;
    uint c_rayListBufferUAV;
    uint c_tileListBufferUAV;
    uint c_rayCounterBufferUAV;
};

void IncrementRayCounter(uint value, out uint original_value)
{
    RWBuffer<uint> rayCounterBufferUAV = ResourceDescriptorHeap[c_rayCounterBufferUAV];
    InterlockedAdd(rayCounterBufferUAV[0], value, original_value);
}

void IncrementDenoiserTileCounter(out uint original_value)
{
    RWBuffer<uint> rayCounterBufferUAV = ResourceDescriptorHeap[c_rayCounterBufferUAV];
    InterlockedAdd(rayCounterBufferUAV[1], 1, original_value);
}

void StoreRay(int index, uint2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal)
{
    RWBuffer<uint> rayListBufferUAV = ResourceDescriptorHeap[c_rayListBufferUAV];
    rayListBufferUAV[index] = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal);
}

void StoreDenoiserTile(int index, uint2 tile_coord)
{
    RWBuffer<uint> tileListBufferUAV = ResourceDescriptorHeap[c_tileListBufferUAV];
    tileListBufferUAV[index] = ((tile_coord.y & 0xffffu) << 16) | ((tile_coord.x & 0xffffu) << 0);
}

bool IsBaseRay(uint2 dispatch_thread_id, uint samples_per_quad)
{
    switch (samples_per_quad)
    {
        case 1:
            return ((dispatch_thread_id.x & 1) | (dispatch_thread_id.y & 1)) == 0; // Deactivates 3 out of 4 rays
        case 2:
            return (dispatch_thread_id.x & 1) == (dispatch_thread_id.y & 1); // Deactivates 2 out of 4 rays. Keeps diagonal.
        default: // case 4:
            return true;
    }
}

groupshared uint s_TileCount;

[numthreads(8, 8, 1)]
void main(uint2 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex, uint3 dispatchThreadID : SV_DispatchThreadID)
{
    s_TileCount = 0;

#if GFX_VENDOR_AMD
#define HSR_REMAP_LANE 0 //todo : remap seems not working correctly on AMD
#else
#define HSR_REMAP_LANE 1
#endif

#if HSR_REMAP_LANE
    // Remap lanes to ensure four neighboring lanes are arranged in a quad pattern
    uint2 group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(groupIndex);
    uint2 dispatch_thread_id = groupID * 8 + group_thread_id;
#else
    uint2 dispatch_thread_id = dispatchThreadID.xy;
#endif
    
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    
    float roughness = normalTexture[dispatch_thread_id].w;
    float depth = depthTexture[dispatch_thread_id];
    
    bool is_first_lane_of_wave = WaveIsFirstLane();
    bool needs_ray = !(dispatch_thread_id.x >= SceneCB.viewWidth || dispatch_thread_id.y >= SceneCB.viewHeight);
    
    bool is_reflective_surface = depth > 0.0;
    bool is_glossy_reflection = FFX_DNSR_Reflections_IsGlossyReflection(roughness);
    needs_ray = needs_ray && is_glossy_reflection && is_reflective_surface;

    bool needs_denoiser = needs_ray && !FFX_DNSR_Reflections_IsMirrorReflection(roughness);
    
    bool is_base_ray = IsBaseRay(dispatch_thread_id, c_samplesPerQuad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray); // Make sure to not deactivate mirror reflection rays.
    
    if (c_bEnableVarianceGuidedTracing && needs_denoiser && !needs_ray)
    {
        Texture2D<float> historyVarianceTexture = ResourceDescriptorHeap[c_historyVarianceTexture];
        const float variance_threshold = 0.001;
        bool has_temporal_variance = historyVarianceTexture[dispatch_thread_id] > variance_threshold;
        needs_ray = needs_ray || has_temporal_variance;
    }
    
    GroupMemoryBarrierWithGroupSync(); // Wait until g_TileCount is cleared 
    
    if (is_glossy_reflection && is_reflective_surface)
    {
        InterlockedAdd(s_TileCount, 1);
    }
    
    bool require_copy = !needs_ray && needs_denoiser; // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
    
#if 0 //HSR_REMAP_LANE
    bool x_require_copy = WaveReadLaneAt(require_copy, WaveGetLaneIndex() ^ 0b01);
    bool y_require_copy = WaveReadLaneAt(require_copy, WaveGetLaneIndex() ^ 0b10);
    bool d_require_copy = WaveReadLaneAt(require_copy, WaveGetLaneIndex() ^ 0b11);
#else
    bool x_require_copy = (bool)QuadReadAcrossX(require_copy);
    bool y_require_copy = (bool)QuadReadAcrossY(require_copy);
    bool d_require_copy = (bool)QuadReadAcrossDiagonal(require_copy);
#endif

    bool copy_horizontal = (c_samplesPerQuad != 4) && is_base_ray && x_require_copy;
    bool copy_vertical = (c_samplesPerQuad == 1) && is_base_ray && y_require_copy;
    bool copy_diagonal = (c_samplesPerQuad == 1) && is_base_ray && d_require_copy;
    
    uint local_ray_index_in_wave = WavePrefixCountBits(needs_ray);
    uint wave_ray_count = WaveActiveCountBits(needs_ray);
    uint base_ray_index;
    if (is_first_lane_of_wave)
    {
        IncrementRayCounter(wave_ray_count, base_ray_index);
    }
    
    base_ray_index = WaveReadLaneFirst(base_ray_index);
    if (needs_ray)
    {
        int ray_index = base_ray_index + local_ray_index_in_wave;
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
    

    GroupMemoryBarrierWithGroupSync(); // Wait until g_TileCount
    
    if (groupIndex == 0 && s_TileCount > 0)
    {
        uint tile_offset;
        IncrementDenoiserTileCounter(tile_offset);
        StoreDenoiserTile(tile_offset, dispatch_thread_id.xy);
    }
}