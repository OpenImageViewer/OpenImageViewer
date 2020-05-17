#CodecGif

cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)


file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecGif)
add_library (${TargetName} ${sourceFiles})
include_directories(d:/DevelopmentExternal/GifLib/GifLib/GifLib/Include)
target_link_libraries(${TargetName} D:/DevelopmentExternal/GifLib/GifLib/x64/Debug/GifLib.lib)