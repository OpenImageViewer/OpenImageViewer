#OIV library
cmake_minimum_required(VERSION 3.10)



#todo fix it.
set(ExternalFolder ../External)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_default_d3d11 ON)
    set(_default_gl OFF)
else()
    set(_default_d3d11 OFF)
    set(_default_gl ON)
endif()

option(OIV_BUILD_RENDERER_D3D11 "Build D3D11 renderer" ${_default_d3d11})
option(OIV_BUILD_RENDERER_GL "Build OpenGL renderer" ${_default_gl})


include_directories(./OIV/Include)
if(OIV_BUILD_RENDERER_D3D11)
    include_directories(Renderers/OIVD3D11Renderer/Include)
endif()
if(OIV_BUILD_RENDERER_GL)
    include_directories(Renderers/OIVGLRenderer)
endif()
include_directories(${ExternalFolder}/ExoticNumbers/include)
include_directories(${ExternalFolder}/ImageCodec/ImageCodec/Include)

add_subdirectory(OIV)
add_subdirectory(Renderers)
