#OIV library
#cmake_minimum_required(VERSION 3.8)

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(ExternalFolder ../../External)
set(TargetName oiv)


add_library (${TargetName} STATIC ${sourceFiles})

option(OIV_BUILD_FREETYPE "build freetype" ON)


target_compile_definitions(${TargetName} PRIVATE OIV_LIBRARY_BUILD)

if ( ${OIV_BUILD_FREETYPE})
	target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_FREETYPE=1)
endif()

target_include_directories(${TargetName} PRIVATE ./Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/Image/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageLoader/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/easyexif)

option(IMCODEC_BUILD_EXAMPLES "Build Codec FREEIMAGE" FALSE)
add_subdirectory(${ExternalFolder}/ImageCodec ./external/ImageCodec)
add_subdirectory(${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper ./external/FreeTypeWrapper)
add_subdirectory(${ExternalFolder}/easyexif ./external/easyexif)

target_link_libraries(${TargetName} 
#Local libraries
ImageCodec
easyexif
FreeTypeWrapper
OIVD3D11Renderer
dbghelp
delayimp

#external dependencies

)