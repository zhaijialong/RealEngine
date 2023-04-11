set(SHADER_ROOT ${REAL_ENGINE_ROOT}/shaders)

file(GLOB_RECURSE SHADER_FILES
    ${SHADER_ROOT}/shaders.cmake
    "${SHADER_ROOT}/*.hlsl"
    "${SHADER_ROOT}/*.hlsli"
    "${SHADER_ROOT}/*.h"
)

set_source_files_properties(${SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

source_group(TREE ${SHADER_ROOT} PREFIX shaders FILES ${SHADER_FILES})