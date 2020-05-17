#Codec DDS
cmake_minimum_required(VERSION 3.8)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName ImageLoader)
add_library (${TargetName} ${sourceFiles} )
target_link_libraries(${TargetName}
CodecDDS
CodecFreeImage
CodecGif
CodecJPG
CodecPNG
CodecPSD
CodecTiff
)



