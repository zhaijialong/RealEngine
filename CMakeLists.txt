cmake_minimum_required(VERSION 3.16)
project(RealEngine C CXX)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_definitions(/MP)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
    add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions -Wno-deprecated-builtins -Wno-nullability-completeness")
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

# OffsetAllocator
add_library(OffsetAllocator ${EXTERNAL_ROOT}/OffsetAllocator/offsetAllocator.cpp ${EXTERNAL_ROOT}/OffsetAllocator/offsetAllocator.hpp)
set_target_properties(OffsetAllocator PROPERTIES FOLDER External CXX_STANDARD 20)

# RealEngine
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(RealEngine WIN32 ${ENGINE_SRC_FILES} ${EXTERNAL_FILES} ${SHADER_FILES})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_executable(RealEngine MACOSX_BUNDLE ${ENGINE_SRC_FILES} ${EXTERNAL_FILES} ${SHADER_FILES})
endif()

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

target_compile_definitions(RealEngine PUBLIC
    MICROPROFILE_ENABLED=1
    MICROPROFILE_MAX_THREADS=64
    MICROPROFILE_GPU_TIMERS=0
    EASTL_EASTDC_VSNPRINTF=0
    EASTL_USER_DEFINED_ALLOCATOR=1
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)
target_link_libraries(RealEngine Jolt OffsetAllocator)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # NRD
    set(NRD_EMBEDS_SPIRV_SHADERS OFF)
    set(NRD_EMBEDS_DXIL_SHADERS ON)
    set(NRD_EMBEDS_DXBC_SHADERS OFF)
    set(DXC_PATH ${REAL_ENGINE_ROOT}/bin/dxc.exe)
    set(DXC_SPIRV_PATH ${REAL_ENGINE_ROOT}/bin/dxc.exe)
    set(NRD_SHADERS_PATH ${CMAKE_CURRENT_BINARY_DIR}/NRD_Shaders)
    set(GLOBAL_BIN_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/NRD)
    add_subdirectory(${EXTERNAL_ROOT}/RayTracingDenoiser)	
    set_target_properties(NRD PROPERTIES FOLDER	External)
    set_target_properties(NRD_Shaders PROPERTIES FOLDER External)
    set_target_properties(ShaderMake PROPERTIES FOLDER External)
    set_target_properties(ShaderMakeBlob PROPERTIES FOLDER External)

    target_link_directories(RealEngine PUBLIC
        ${EXTERNAL_ROOT}/DLSS/lib/Windows_x86_64/x86_64
        ${EXTERNAL_ROOT}/xess/lib/
    )

    target_link_libraries(RealEngine NRD ws2_32)

    add_custom_command(
        TARGET RealEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/DLSS/lib/Windows_x86_64/rel/nvngx_dlss.dll ${REAL_ENGINE_ROOT}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/libxess.dll ${REAL_ENGINE_ROOT}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/igxess.dll ${REAL_ENGINE_ROOT}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX_Loader.dll ${REAL_ENGINE_ROOT}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/xess/bin/XeFX.dll ${REAL_ENGINE_ROOT}/bin
    )

    # oidn
    if(EXISTS ${EXTERNAL_ROOT}/oidn/include/OpenImageDenoise/oidn.h)
        target_compile_definitions(RealEngine PUBLIC WITH_OIDN=1)
        target_include_directories(RealEngine PUBLIC ${EXTERNAL_ROOT}/oidn/include)
        target_link_directories(RealEngine PUBLIC ${EXTERNAL_ROOT}/oidn/lib)
        target_link_libraries(RealEngine OpenImageDenoise)

        add_custom_command(
            TARGET RealEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_core.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_cpu.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_cuda.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_hip.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_sycl.dll ${REAL_ENGINE_ROOT}/bin
            #COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_sycl_xehpg.dll ${REAL_ENGINE_ROOT}/bin
            #COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/OpenImageDenoise_device_sycl_xelp.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/pi_level_zero.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/pi_win_proxy_loader.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/sycl7.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/tbb12.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/tbbbind.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/tbbbind_2_0.dll ${REAL_ENGINE_ROOT}/bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_ROOT}/oidn/bin/tbbbind_2_5.dll ${REAL_ENGINE_ROOT}/bin
        )
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(RealEngine PUBLIC
        IMGUI_IMPL_METAL_CPP_EXTENSIONS=1
    )

    target_include_directories(RealEngine PUBLIC
        ${EXTERNAL_ROOT}/metal-cpp
        ${EXTERNAL_ROOT}/MetalShaderConverter/include
    )

    set_target_properties(RealEngine PROPERTIES
        BUNDLE True
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks"
        XCODE_EMBED_FRAMEWORKS ${EXTERNAL_ROOT}/MetalShaderConverter/lib/libmetalirconverter.dylib
        MACOSX_BUNDLE_GUI_IDENTIFIER com.zhaijialong.RealEngine
        MACOSX_BUNDLE_BUNDLE_NAME RealEngine
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_INFO_PLIST ${SOURCE_ROOT}/main/mac/info.plist.in
    )

    target_link_libraries(RealEngine
        ${EXTERNAL_ROOT}/MetalShaderConverter/lib/libmetalirconverter.dylib
        "-framework Foundation"
        "-framework AppKit"
        "-framework Metal"
        "-framework MetalKit"
        "-framework GameController"
    )
endif()