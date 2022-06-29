#include "ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211.h"

typedef union ffx_fsr2_prepare_input_color_pass_wave64_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_prepare_input_color_pass_wave64_PermutationKey;

typedef struct ffx_fsr2_prepare_input_color_pass_wave64_PermutationInfo {
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
} ffx_fsr2_prepare_input_color_pass_wave64_PermutationInfo;

static const uint32_t g_ffx_fsr2_prepare_input_color_pass_wave64_IndirectionTable[] = {
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    3,
    3,
    1,
    1,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
    2,
    2,
    0,
    0,
};

static const ffx_fsr2_prepare_input_color_pass_wave64_PermutationInfo g_ffx_fsr2_prepare_input_color_pass_wave64_PermutationInfo[] = {
    { g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_size, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8cc8e967ebff38f189a0487fbfc8caf2_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_size, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_1124b092bf70786c7b9e0487fbede2ea_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_size, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_afa73bc787a14d4405c19e29f4b90690_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_size, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_8f44ae8bd8c5c97ab2db5b8e15f98211_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

