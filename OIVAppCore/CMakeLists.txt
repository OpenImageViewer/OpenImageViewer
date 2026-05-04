file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName oivappcore)

add_library(${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./Source/Include)
target_include_directories(${TargetName} PRIVATE ./Source/Src)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ../OIVLib/OIV/Source)

target_link_libraries(${TargetName}
PUBLIC
oivshared
oiv
ImageUtil
ImageCodec
)
