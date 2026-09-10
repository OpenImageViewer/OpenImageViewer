
# Validate Debug/Ninja builds in build/windows-clang (Visual Studio dev shell) and build/linux-clang (WSL).
# Windows also runs tests_vk_runtime_failure, with .exe suffixes on all four executables.
# Renderer-policy changes need defaults, each renderer alone, Windows all-renderer, and test-only Null checks.

file(GLOB_RECURSE sourceFiles CONFIGURE_DEPENDS
    "./Source/*.h"
    "./Source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName tests)

if(NOT TARGET Catch2::Catch2WithMain)
    add_subdirectory(${ExternalFolder}/catch2 ./external/catch2)
endif()

# FreeType's native PNG dependency and ImageCodec's APNG library export overlapping
# symbols on Linux. Exercise the font boundary in its own executable.
add_executable(tests_font_encoding FontEncoding/TestFontEncoding.cpp)
target_include_directories(tests_font_encoding PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_compile_definitions(tests_font_encoding PRIVATE OIV_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\")
target_link_libraries(tests_font_encoding PRIVATE Catch2::Catch2WithMain FreeTypeWrapper LLUtils::LLUtils)

add_executable (${TargetName} ${sourceFiles})
target_sources(${TargetName} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/Source/ImageList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/Source/MouseGestureController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/Source/MouseMultiClickHandler.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/LInput/Include)
    target_sources(${TargetName} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/SrcPlatform/Win32/ContextMenuBackend.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${TargetName} PRIVATE OIV_TEST_PLATFORM_LINUX)
    if(NOT TARGET PkgConfig::GTK3)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    endif()
    pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
    target_link_libraries(${TargetName} PRIVATE PkgConfig::GTK3 PkgConfig::WaylandClient)
    target_sources(${TargetName} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/SrcPlatform/Linux/PlatformFileDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/SrcPlatform/Linux/ContextMenuBackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/SrcPlatform/Linux/LinuxKeyBindings.cpp
    )
endif()
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/include)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer/Source)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../OIVLib/OIV/Source)
target_compile_definitions(${TargetName} PRIVATE OIV_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\")
target_link_libraries(${TargetName} PRIVATE Catch2::Catch2WithMain oivshared oivappcore ImageCodec tiff LWSLib)

if(OIV_BUILD_RENDERER_GL)
    target_compile_definitions(${TargetName} PRIVATE OIV_TEST_GL_RENDERER)
    target_link_libraries(${TargetName} PRIVATE OIVGLRenderer)
endif()

if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_link_options(${TargetName} PRIVATE "/SUBSYSTEM:CONSOLE")
    target_link_options(${TargetName} PRIVATE "/ENTRY:wmainCRTStartup")
    target_link_options(tests_font_encoding PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
elseif(MINGW)
    set_target_properties(${TargetName} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_options(${TargetName} PRIVATE "-mconsole" "-municode")
    target_link_options(tests_font_encoding PRIVATE "-mconsole" "-municode")
endif()
