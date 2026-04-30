file(GLOB_RECURSE sourceFiles
    "./source/*.h"
    "./source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName oivshared)

add_library (${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./source/include)
target_include_directories(${TargetName} PRIVATE ./source/src)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_link_libraries(${TargetName} PUBLIC LLUtils::LLUtils ImageCodec)
