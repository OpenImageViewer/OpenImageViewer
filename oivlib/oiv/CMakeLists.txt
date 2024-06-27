#OIV library
#cmake_minimum_required(VERSION 3.8)

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Include/*.h"
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
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/easyexif)

option(IMCODEC_BUILD_EXAMPLES "Build Codec FREEIMAGE" FALSE)
add_subdirectory(${ExternalFolder}/ImageCodec ./external/ImageCodec)


option(FREETYPE_WRAPPER_BUILD_SAMPLES "freetype build samples" OFF)

add_subdirectory(${ExternalFolder}/FreeTypeWrapper/ ./external/FreeTypeWrapper)

option(EASY_EXIF_BUILD_DEMO "build exif demo" OFF)

add_subdirectory(${ExternalFolder}/easyexif ./external/easyexif)

if (OIV_DISABLE_WARNINGS_EXTERNAL_LIBS)
    target_compile_definitions(easyexif PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

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