#OIV library
cmake_minimum_required(VERSION 3.10)



#todo fix it.
set(ExternalFolder ../External)

include_directories(./oiv/Include)
include_directories(renderers/OIVD3D11Renderer/Include)
include_directories(${ExternalFolder}/ExoticNumbers/include)
include_directories(${ExternalFolder}/LLUtils/Include)
include_directories(${ExternalFolder}/ImageCodec/ImageCodec/Include)

add_subdirectory(oiv)
add_subdirectory(renderers)
