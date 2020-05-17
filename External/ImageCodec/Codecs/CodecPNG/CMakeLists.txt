#CodecPNG

cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecPNG)
add_library (${TargetName} ${sourceFiles} )
include_directories(${TargetName} d:/DevelopmentExternal/libpng-code  )
target_link_libraries(${TargetName} d:/DevelopmentExternal/libpng-code/projects/vstudio/x64/Release/libpng16.lib)
