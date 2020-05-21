cmake_minimum_required(VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecFreeImage)
add_library (${TargetName} ${sourceFiles} )
target_link_libraries(${TargetName} d:/DevelopmentExternal/FreeImage3180Win32Win64/FreeImage/Dist/x64/FreeImage.lib)
include_directories(D:/DevelopmentExternal/FreeImage3180Win32Win64/FreeImage/Dist/x64)