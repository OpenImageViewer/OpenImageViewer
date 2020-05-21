#Codec JPG
cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecJPG)
add_library (${TargetName} ${sourceFiles} )
include_directories(D:/DevelopmentExternal/libjpeg-turbo64/include)

target_link_libraries(${TargetName} d:/DevelopmentExternal/libjpeg-turbo64/lib/turbojpeg.lib )