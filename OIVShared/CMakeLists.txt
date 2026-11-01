file(GLOB_RECURSE sourceFiles CONFIGURE_DEPENDS
    "./Source/*.h"
    "./Source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

option(FREETYPE_WRAPPER_BUILD_SAMPLES "Build FreeTypeWrapper samples" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FREETYPE_WRAPPER_BUILD_FRIBIDI OFF CACHE BOOL "Build FriBidi" FORCE)
endif()
add_subdirectory("${ExternalFolder}/FreeTypeWrapper" "${CMAKE_CURRENT_BINARY_DIR}/external/FreeTypeWrapper")

set(TargetName oivshared)

option(OIV_BUILD_FREETYPE "Build FreeType text rasterization" ON)

add_library (${TargetName} STATIC ${sourceFiles})

target_include_directories(${TargetName} PUBLIC ./Source/Include)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PUBLIC ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ./Source/Src)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_FREETYPE=$<BOOL:${OIV_BUILD_FREETYPE}>)
target_link_libraries(${TargetName} PUBLIC LLUtils::LLUtils ImageCodec ImageUtil FreeTypeWrapper)

if(OIV_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
