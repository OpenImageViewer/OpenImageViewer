#OIV library
#cmake_minimum_required(VERSION 3.8)

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Source/*.cpp"
 )

set(ExternalFolder ../../External)
set(TargetName oiv)
add_library (${TargetName} SHARED ${sourceFiles} )

set_target_properties(${TargetName} PROPERTIES LINK_FLAGS 
"/DELAYLOAD:\"libpng16.dll\" /DELAYLOAD:\"FreeImage.dll\" /DELAYLOAD:\"D3DCompiler_47.dll\" /DELAYLOAD:\"dbghelp.dll\" /DELAYLOAD:\"turbojpeg.dll\"")
target_compile_definitions(${TargetName} PRIVATE OIV_LIBRARY_BUILD)

target_include_directories(${TargetName} PRIVATE ./Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/Image/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageLoader/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/easyexif)

add_subdirectory(${ExternalFolder}/ImageCodec ./external/ImageCodec)
add_subdirectory(${ExternalFolder}/FreeTypeWrapper ./external/FreeTypeWrapper)
add_subdirectory(${ExternalFolder}/easyexif ./external/easyexif)

target_link_libraries(${TargetName} 
#Local libraries
ImageLoader
easyexif
FreeTypeWrapper
OIVD3D11Renderer
dbghelp
delayimp

#external dependencies

)