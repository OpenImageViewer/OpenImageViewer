file(GLOB_RECURSE sourceFiles CONFIGURE_DEPENDS
    "./Source/*.h"
    "./Source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName oivshared)

add_library (${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./Source/Include)
target_include_directories(${TargetName} PRIVATE ./Source/Src)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_link_libraries(${TargetName} PUBLIC LLUtils::LLUtils ImageCodec)

# Decimal precision selection relies on IEEE rounding and subnormal values under LTO too.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_source_files_properties(Source/Src/UnitFormatter.cpp PROPERTIES COMPILE_OPTIONS "/fp:precise")
endif()
