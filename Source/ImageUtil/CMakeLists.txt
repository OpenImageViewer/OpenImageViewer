#Codec DDS
cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName ImageUtil)
add_library (${TargetName} ${sourceFiles} )
include_directories(../ImageCodec/Image/Include)

