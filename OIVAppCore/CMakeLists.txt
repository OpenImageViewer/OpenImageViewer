file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName oivappcore)

add_library(${TargetName} STATIC ${sourceFiles})

# These input boundaries classify nonfinite numbers and saturate animation intervals. Override
# the viewer's /fp:fast for these files, including their standard-library inline definitions.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_source_files_properties(
        Source/Src/SequencerPolicy.cpp
        Source/Src/AppSettingsPolicy.cpp
        PROPERTIES COMPILE_OPTIONS "/fp:precise"
    )
endif()

target_include_directories(${TargetName} PUBLIC ./Source/Include)
target_include_directories(${TargetName} PRIVATE ./Source/Src)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ../OIVLib/OIV/Source)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ExoticNumbers/include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/single_include)

target_link_libraries(${TargetName}
PUBLIC
oivshared
oiv
ImageUtil
ImageCodec
)
