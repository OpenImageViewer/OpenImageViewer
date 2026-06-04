#OIV library
#cmake_minimum_required(VERSION 3.8)

 file(GLOB_RECURSE sourceFiles
     "./Source/*.h"
     "./Include/*.h"
     "./Source/*.cpp"
 )

set(ExternalFolder ../../External)
set(TargetName oiv)


add_library (${TargetName} STATIC ${sourceFiles})

option(OIV_BUILD_FREETYPE "build freetype" ON)


target_compile_definitions(${TargetName} PRIVATE OIV_LIBRARY_BUILD)

if ( ${OIV_BUILD_FREETYPE})
	target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_FREETYPE=1)
endif()

target_include_directories(${TargetName}
    PUBLIC
        ./Include
        ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include
    PRIVATE
        ${ExternalFolder}/easyexif
)


option(FREETYPE_WRAPPER_BUILD_SAMPLES "freetype build samples" OFF)

add_subdirectory(${ExternalFolder}/FreeTypeWrapper/ ./external/FreeTypeWrapper)

option(EASY_EXIF_BUILD_DEMO "build exif demo" OFF)

add_subdirectory(${ExternalFolder}/easyexif ./external/easyexif)

if (OIV_DISABLE_WARNINGS_EXTERNAL_LIBS)
    target_compile_definitions(easyexif PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(${TargetName}
PUBLIC
LLUtils::LLUtils
ImageUtil
ImageCodec
PRIVATE
#Local libraries
easyexif
FreeTypeWrapper
#external dependencies
)

if(OIV_BUILD_RENDERER_D3D11)
    target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_RENDERER_D3D11=1)
    target_link_libraries(${TargetName}
    PRIVATE
        OIVD3D11Renderer
        delayimp
    )
endif()

if(OIV_BUILD_RENDERER_GL)
    target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_RENDERER_GL=1)
    target_link_libraries(${TargetName}
    PRIVATE
        OIVGLRenderer
    )
else()
    target_compile_definitions(${TargetName} PRIVATE OIV_BUILD_RENDERER_GL=0)
endif()
