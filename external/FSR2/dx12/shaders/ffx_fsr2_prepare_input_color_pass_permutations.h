#include "ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35.h"
#include "ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6.h"
#include "ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87.h"
#include "ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5.h"

typedef union ffx_fsr2_prepare_input_color_pass_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_prepare_input_color_pass_PermutationKey;

typedef struct ffx_fsr2_prepare_input_color_pass_PermutationInfo {
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
} ffx_fsr2_prepare_input_color_pass_PermutationInfo;

static const uint32_t g_ffx_fsr2_prepare_input_color_pass_IndirectionTable[] = {
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

static const ffx_fsr2_prepare_input_color_pass_PermutationInfo g_ffx_fsr2_prepare_input_color_pass_PermutationInfo[] = {
    { g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_size, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_data, 1, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_cd708d81a1115515c856bd9a3338bf35_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_size, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_data, 1, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_2a6619dc78c90771729c91b1734c17a6_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_size, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_data, 1, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_5479be3beda4f974d3ff45410b981d87_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_size, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_data, 1, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_0d06a6bb5e8907ced80724484a2bfee5_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

