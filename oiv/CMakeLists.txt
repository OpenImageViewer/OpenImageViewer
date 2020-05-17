#OIV library
cmake_minimum_required(VERSION 3.8)

#TODO: change source files to be under Source folder
file(GLOB_RECURSE sourceFiles
    "./*.h"
    "./*.cpp"
)

set(TargetName oiv)
add_library (${TargetName} ${sourceFiles} )
target_compile_definitions(${TargetName} PUBLIC OIV_LIBRARY_BUILD)

target_include_directories(${TargetName} PUBLIC ./)
target_include_directories(${TargetName} PUBLIC ../ImageCodec/Image/Include)
target_include_directories(${TargetName} PUBLIC ../ImageCodec/ImageLoader/Include)
target_include_directories(${TargetName} PUBLIC ../ImageUtil/Include)
target_include_directories(${TargetName} PUBLIC D:/DevelopmentExternal/freetype-2.10.0/include)

target_link_libraries(${TargetName} 
#Local libraries
ImageLoader
ImageUtil
OIVD3D11Renderer

#external dependencies
"D:/DevelopmentExternal/freetype-2.10.0/objs/x64/Release Static/freetype.lib"
)
