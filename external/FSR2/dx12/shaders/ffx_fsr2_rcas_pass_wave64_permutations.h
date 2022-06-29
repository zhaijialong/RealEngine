#include "ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee.h"
#include "ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432.h"

typedef union ffx_fsr2_rcas_pass_wave64_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_rcas_pass_wave64_PermutationKey;

typedef struct ffx_fsr2_rcas_pass_wave64_PermutationInfo {
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
} ffx_fsr2_rcas_pass_wave64_PermutationInfo;

static const uint32_t g_ffx_fsr2_rcas_pass_wave64_IndirectionTable[] = {
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

static const ffx_fsr2_rcas_pass_wave64_PermutationInfo g_ffx_fsr2_rcas_pass_wave64_PermutationInfo[] = {
    { g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_size, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_data, 2, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_CBVResourceNames, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_CBVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_CBVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_CBVResourceSpaces, 2, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_SRVResourceNames, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_SRVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_SRVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_SRVResourceSpaces, 1, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_UAVResourceNames, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_UAVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_UAVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_7191b1047169eeeb3955f732898f51ee_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_size, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_data, 2, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_CBVResourceNames, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_CBVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_CBVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_CBVResourceSpaces, 2, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_SRVResourceNames, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_SRVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_SRVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_SRVResourceSpaces, 1, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_UAVResourceNames, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_UAVResourceBindings, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_UAVResourceCounts, g_ffx_fsr2_rcas_pass_wave64_28b9e895c647516fece924ce32f00432_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

