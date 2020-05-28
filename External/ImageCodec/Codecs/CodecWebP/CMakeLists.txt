#Codec WebP
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# Add source to this project's executable.
file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName CodecWebP)
add_library (${TargetName} ${sourceFiles} )
add_subdirectory(D:/DevelopmentExternal/libwebp  ./external/libwebp)
target_include_directories(${TargetName} PRIVATE D:/DevelopmentExternal/libwebp)
target_include_directories(${TargetName} PRIVATE D:/DevelopmentExternal/libwebp/src)

target_link_libraries(${TargetName} 
exampleutil
imagedec
imageenc
imageioutil
webp
webpdecoder
webpdemux
)
