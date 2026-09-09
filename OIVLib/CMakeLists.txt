#OIV library
cmake_minimum_required(VERSION 3.10)



#todo fix it.
set(ExternalFolder ../External)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_default_d3d11 ON)
    set(_default_gl OFF)
    set(_default_vk ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_default_d3d11 OFF)
    set(_default_gl ON)
    set(_default_vk ON)
else()
    set(_default_d3d11 OFF)
    set(_default_gl OFF)
    set(_default_vk OFF)
endif()

# Build inclusion is independent of runtime preference. Defaults are Vulkan+D3D11 on
# Windows and Vulkan+GL on Linux. Optional Windows GL remains supported. Enabled
# renderers require their dependencies; disable one explicitly with its option.
# Runtime exhausts Hardware, Unknown, then Software, using Vulkan -> D3D11 -> GL
# within each tier and omitting implementations not compiled into this library.
option(OIV_BUILD_RENDERER_D3D11 "Build D3D11 renderer" ${_default_d3d11})
option(OIV_BUILD_RENDERER_GL "Build OpenGL renderer" ${_default_gl})
option(OIV_BUILD_RENDERER_VK "Build Vulkan renderer" ${_default_vk})

if(OIV_BUILD_RENDERER_D3D11 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "D3D11 requires a Windows target. Set OIV_BUILD_RENDERER_D3D11=OFF.")
endif()
if((OIV_BUILD_RENDERER_GL OR OIV_BUILD_RENDERER_VK) AND
   NOT CMAKE_SYSTEM_NAME MATCHES "^(Windows|Linux)$")
    message(FATAL_ERROR "GL/Vulkan support Windows and Linux targets only. Disable OIV_BUILD_RENDERER_GL and OIV_BUILD_RENDERER_VK.")
endif()
if(OIV_BUILD_CLIENT AND NOT (OIV_BUILD_RENDERER_D3D11 OR OIV_BUILD_RENDERER_GL OR OIV_BUILD_RENDERER_VK))
    message(FATAL_ERROR "The viewer requires a real renderer. Enable OIV_BUILD_RENDERER_VK, OIV_BUILD_RENDERER_D3D11, or OIV_BUILD_RENDERER_GL; Null is test-only.")
endif()
message(STATUS "OIViewer renderers: Vulkan=${OIV_BUILD_RENDERER_VK}; D3D11=${OIV_BUILD_RENDERER_D3D11}; GL=${OIV_BUILD_RENDERER_GL}")



include_directories(./OIV/Include)
if(OIV_BUILD_RENDERER_D3D11)
    include_directories(Renderers/OIVD3D11Renderer/Include)
endif()
if(OIV_BUILD_RENDERER_GL)
    include_directories(Renderers/OIVGLRenderer)
endif()
if(OIV_BUILD_RENDERER_VK)
    include_directories(Renderers/OIVVKRenderer)
endif()
include_directories(${ExternalFolder}/ExoticNumbers/include)
include_directories(${ExternalFolder}/ImageCodec/ImageCodec/Include)

add_subdirectory(OIV)
add_subdirectory(Renderers)
