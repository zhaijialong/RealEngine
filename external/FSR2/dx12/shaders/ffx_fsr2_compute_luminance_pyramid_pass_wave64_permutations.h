#include "ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f.h"

typedef union ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationKey;

typedef struct ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationInfo {
    const uint32_t       blobSize;
    const unsigned char* blobData;


    const uint32_t  numCBVResources;
    const char**    cbvResourceNames;
    const uint32_t* cbvResourceBindings;
    const uint32_t* cbvResourceCounts;
    const uint32_t* cbvResourceSpaces;

    const uint32_t  numSRVResources;
    const char**    srvResourceNames;
    const uint32_t* srvResourceBindings;
    const uint32_t* srvResourceCounts;
    const uint32_t* srvResourceSpaces;

    const uint32_t  numUAVResources;
    const char**    uavResourceNames;
    const uint32_t* uavResourceBindings;
    const uint32_t* uavResourceCounts;
    const uint32_t* uavResourceSpaces;

    const uint32_t  numSamplerResources;
    const char**    samplerResourceNames;
    const uint32_t* samplerResourceBindings;
    const uint32_t* samplerResourceCounts;
    const uint32_t* samplerResourceSpaces;

    const uint32_t  numRTAccelerationStructureResources;
    const char**    rtAccelerationStructureResourceNames;
    const uint32_t* rtAccelerationStructureResourceBindings;
    const uint32_t* rtAccelerationStructureResourceCounts;
    const uint32_t* rtAccelerationStructureResourceSpaces;
} ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationInfo;

static const uint32_t g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_IndirectionTable[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationInfo g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_PermutationInfo[] = {
    { g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_size, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_data, 2, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_CBVResourceNames, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_CBVResourceBindings, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_CBVResourceCounts, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_CBVResourceSpaces, 1, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_SRVResourceNames, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_SRVResourceBindings, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_SRVResourceCounts, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_SRVResourceSpaces, 4, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_UAVResourceNames, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_UAVResourceBindings, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_UAVResourceCounts, g_ffx_fsr2_compute_luminance_pyramid_pass_wave64_f1c8a418cff787c1a75f3056d24c2a1f_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

