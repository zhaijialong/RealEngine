#include "ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71.h"
#include "ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d.h"

typedef union ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationKey;

typedef struct ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationInfo {
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
} ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationInfo;

static const uint32_t g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_IndirectionTable[] = {
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    3,
    3,
    0,
    0,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
    2,
    2,
    1,
    1,
};

static const ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationInfo g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_PermutationInfo[] = {
    { g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_size, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_15dce498c9524cac380de9f3317f7fb0_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_size, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_f89ff0f7e8dc5badedc2e04ba4ff7a11_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_size, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_059fe08155f5727885364f6fa970fa71_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_size, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_data, 1, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_wave64_16bit_1ac0a3fed597410cf782e12433d0d04d_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

