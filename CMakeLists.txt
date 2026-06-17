# retip - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(retip LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

add_subdirectory(thirdparty/plume)
include(thirdparty/plume/examples/cmake/PlumeShaders.cmake)
plume_shaders_init()

# Sources
set(RETIP_SOURCES
    src/main.cpp
    src/Renderer/plume_renderer.cpp
    src/Renderer/video_hooks.cpp
    src/Debug/trace_hooks.cpp
)

if(WIN32)
    add_executable(retip WIN32 ${RETIP_SOURCES})
else()
    add_executable(retip ${RETIP_SOURCES})
endif()

rexglue_setup_target(retip)

target_link_libraries(retip PRIVATE plume)
if(WIN32)
    target_link_libraries(retip PRIVATE d3d12 dxgi)
endif()

plume_compile_vertex_shader(retip
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video.vert.hlsl videoVert VSMain)
plume_compile_pixel_shader(retip
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video.frag.hlsl videoFrag PSMain)

set_source_files_properties(src/plume_renderer.cpp PROPERTIES
    OBJECT_DEPENDS
    "${CMAKE_BINARY_DIR}/shaders/videoVert.hlsl.spirv.h;\
${CMAKE_BINARY_DIR}/shaders/videoFrag.hlsl.spirv.h;\
${CMAKE_BINARY_DIR}/shaders/videoVert.hlsl.dxil.h;\
${CMAKE_BINARY_DIR}/shaders/videoFrag.hlsl.dxil.h"
)
