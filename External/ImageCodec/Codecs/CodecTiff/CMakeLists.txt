
cmake_minimum_required(VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecTiff)
add_library (${TargetName}  ${sourceFiles} )
include_directories(${TargetName} d:/DevelopmentExternal/libtiff/libtiff)
target_link_libraries(${TargetName} d:/DevelopmentExternal/libtiff/lib/x64/Debug/libtiff.lib)
