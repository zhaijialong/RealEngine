cmake_minimum_required(VERSION 3.16)
project(RealEngine C CXX)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_definitions(/MP)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
    
    # todo : why /Ob2 not working ?
    string(REPLACE "/Ob2" "/Ob1" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/Ob2" "/Ob1" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
endif()

set(REAL_ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${REAL_ENGINE_ROOT}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${REAL_ENGINE_ROOT}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${REAL_ENGINE_ROOT}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${REAL_ENGINE_ROOT}/bin)

include(${REAL_ENGINE_ROOT}/source/source.cmake)
include(${REAL_ENGINE_ROOT}/shaders/shaders.cmake)
include(${REAL_ENGINE_ROOT}/external/external.cmake)

add_executable(RealEngine WIN32 ${ENGINE_SRC_FILES} ${EXTERNAL_FILES} ${SHADER_FILES})
target_include_directories(RealEngine PUBLIC 
    ${SOURCE_ROOT}
    ${SHADER_ROOT}
    ${EXTERNAL_ROOT}
    ${EXTERNAL_ROOT}/EASTL/include
    ${EXTERNAL_ROOT}/fmt/include
    ${EXTERNAL_ROOT}/stb
    ${EXTERNAL_ROOT}/imgui
    ${EXTERNAL_ROOT}/rpmalloc
    ${EXTERNAL_ROOT}/DLSS/include
    ${EXTERNAL_ROOT}/xess/inc
)
target_link_directories(RealEngine PUBLIC
    ${EXTERNAL_ROOT}/DLSS/lib/Windows_x86_64/x86_64
    ${EXTERNAL_ROOT}/xess/lib/
)
target_compile_definitions(RealEngine PUBLIC
    MICROPROFILE_ENABLED=1
    MICROPROFILE_GPU_TIMERS=1
    MICROPROFILE_GPU_TIMERS_D3D12=1
    EASTL_EASTDC_VSNPRINTF=0
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)
target_link_libraries(RealEngine ws2_32)

add_custom_command(
    TARGET RealEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/DLSS/lib/Windows_x86_64/rel/nvngx_dlss.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/libxess.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/igxess.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX_Loader.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX.dll ${REAL_ENGINE_ROOT}/bin
)
