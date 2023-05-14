cmake_minimum_required(VERSION 3.16)
project(RealEngine C CXX)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_definitions(/MP)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(REAL_ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${REAL_ENGINE_ROOT}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${REAL_ENGINE_ROOT}/bin)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(${REAL_ENGINE_ROOT}/source/source.cmake)
include(${REAL_ENGINE_ROOT}/shaders/shaders.cmake)
include(${REAL_ENGINE_ROOT}/external/external.cmake)

# Jolt
set(USE_AVX OFF)
set(USE_AVX2 OFF)
set(USE_AVX512 OFF)
add_subdirectory(${EXTERNAL_ROOT}/JoltPhysics/Build)
set_target_properties(Jolt PROPERTIES FOLDER External)

# NRD
set(DXC_PATH ${REAL_ENGINE_ROOT}/bin/dxc.exe)
set(DXC_SPIRV_PATH ${REAL_ENGINE_ROOT}/bin/dxc.exe)
set(NRD_SHADERS_PATH ${CMAKE_CURRENT_BINARY_DIR}/NRD_Shaders)
set(GLOBAL_BIN_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/NRD)
add_subdirectory(${EXTERNAL_ROOT}/RayTracingDenoiser)	
set_target_properties(NRD PROPERTIES FOLDER	External)
set_target_properties(NRD_Shaders PROPERTIES FOLDER External)
set_target_properties(ShaderMake PROPERTIES FOLDER External)
set_target_properties(ShaderMakeBlob PROPERTIES FOLDER External)

# RealEngine
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
    ${EXTERNAL_ROOT}/RayTracingDenoiser/Include
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
    EASTL_USER_DEFINED_ALLOCATOR=1
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)
target_link_libraries(RealEngine ws2_32 Jolt NRD)

add_custom_command(
    TARGET RealEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/DLSS/lib/Windows_x86_64/rel/nvngx_dlss.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/libxess.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/igxess.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX_Loader.dll ${REAL_ENGINE_ROOT}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX.dll ${REAL_ENGINE_ROOT}/bin
)
