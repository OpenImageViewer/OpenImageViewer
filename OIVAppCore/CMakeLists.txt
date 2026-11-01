file(GLOB_RECURSE sourceFiles CONFIGURE_DEPENDS
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
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ExoticNumbers/include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/single_include)
target_compile_definitions(${TargetName} PRIVATE
    OIV_BUILD_RENDERER_D3D11=$<BOOL:${OIV_BUILD_RENDERER_D3D11}>
    OIV_BUILD_RENDERER_GL=$<BOOL:${OIV_BUILD_RENDERER_GL}>
    OIV_BUILD_RENDERER_VK=$<BOOL:${OIV_BUILD_RENDERER_VK}>
    OIV_ALLOW_NULL_RENDERER=$<BOOL:${OIV_BUILD_TESTS}>
)

target_link_libraries(${TargetName}
PUBLIC
oivshared
RendererCore
ImageUtil
ImageCodec
)

if(OIV_BUILD_RENDERER_D3D11)
    target_link_libraries(${TargetName} PRIVATE OIVD3D11Renderer)
endif()
if(OIV_BUILD_RENDERER_GL)
    target_link_libraries(${TargetName} PRIVATE OIVGLRenderer)
endif()
if(OIV_BUILD_RENDERER_VK)
    target_link_libraries(${TargetName} PRIVATE OIVVKRenderer)
endif()

if(OIV_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
