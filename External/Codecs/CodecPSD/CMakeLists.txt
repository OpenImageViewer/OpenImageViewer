cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecPSD)
add_library (${TargetName} ${sourceFiles} )
include_directories(d:/DevelopmentExternal/libpsd/include)
target_link_libraries(${TargetName} d:/DevelopmentExternal/libpsd/build/libPSDLibVC142/x64/Debug/libPSDLib.lib)