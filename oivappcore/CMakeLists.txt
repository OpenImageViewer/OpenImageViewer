file(GLOB_RECURSE sourceFiles
    "./source/*.h"
    "./source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName oivappcore)

add_library(${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./source/include)
target_include_directories(${TargetName} PRIVATE ./source/src)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PUBLIC ../oivlib/oiv/Include)
target_include_directories(${TargetName} PRIVATE ../oivlib/oiv/Source)

target_link_libraries(${TargetName}
PUBLIC
oivshared
oiv
ImageUtil
ImageCodec
)
