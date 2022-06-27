#include "ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7.h"
#include "ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351.h"

typedef union ffx_fsr2_rcas_pass_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_rcas_pass_PermutationKey;

typedef struct ffx_fsr2_rcas_pass_PermutationInfo {
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
} ffx_fsr2_rcas_pass_PermutationInfo;

static const uint32_t g_ffx_fsr2_rcas_pass_IndirectionTable[] = {
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
};

static const ffx_fsr2_rcas_pass_PermutationInfo g_ffx_fsr2_rcas_pass_PermutationInfo[] = {
    { g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_size, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_data, 2, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_CBVResourceNames, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_CBVResourceBindings, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_CBVResourceCounts, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_CBVResourceSpaces, 2, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_SRVResourceNames, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_SRVResourceBindings, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_SRVResourceCounts, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_SRVResourceSpaces, 1, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_UAVResourceNames, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_UAVResourceBindings, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_UAVResourceCounts, g_ffx_fsr2_rcas_pass_c57f592fe8cf8d40424ac1d5293587b7_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_size, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_data, 2, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_CBVResourceNames, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_CBVResourceBindings, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_CBVResourceCounts, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_CBVResourceSpaces, 2, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_SRVResourceNames, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_SRVResourceBindings, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_SRVResourceCounts, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_SRVResourceSpaces, 1, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_UAVResourceNames, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_UAVResourceBindings, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_UAVResourceCounts, g_ffx_fsr2_rcas_pass_4f9044c715e44a931b04126142519351_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

