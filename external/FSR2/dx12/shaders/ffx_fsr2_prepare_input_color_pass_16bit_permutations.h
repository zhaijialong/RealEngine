#include "ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c.h"
#include "ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9.h"
#include "ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f.h"
#include "ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76.h"

typedef union ffx_fsr2_prepare_input_color_pass_16bit_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_USE_LANCZOS_LUT : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_prepare_input_color_pass_16bit_PermutationKey;

typedef struct ffx_fsr2_prepare_input_color_pass_16bit_PermutationInfo {
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
} ffx_fsr2_prepare_input_color_pass_16bit_PermutationInfo;

static const uint32_t g_ffx_fsr2_prepare_input_color_pass_16bit_IndirectionTable[] = {
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

static const ffx_fsr2_prepare_input_color_pass_16bit_PermutationInfo g_ffx_fsr2_prepare_input_color_pass_16bit_PermutationInfo[] = {
    { g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_size, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_data, 1, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7bf87a4b81e0e092a3d24c2e9e9a358c_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_size, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_data, 1, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_ed4709651979fb77a074e826383472a9_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_size, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_data, 1, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_7ca7ab7c02efbbb930e8606e6a0a8e7f_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_size, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_data, 1, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_CBVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_CBVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_CBVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_CBVResourceSpaces, 2, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_SRVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_SRVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_SRVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_SRVResourceSpaces, 3, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_UAVResourceNames, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_UAVResourceBindings, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_UAVResourceCounts, g_ffx_fsr2_prepare_input_color_pass_16bit_26beb170a7482fb9d231fe92c09bea76_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

