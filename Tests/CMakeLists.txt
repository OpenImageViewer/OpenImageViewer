# Cross-module integration tests and project-level test utilities stay in this directory.
file(GLOB integrationTestSources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/Test*.cpp")

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

add_executable(tests
    ${integrationTestSources}
    RendererIntegration/TestViewerSession.cpp
    ../External/ImageCodec/Tests/ImageMagickTestCorpus.cpp
)
target_include_directories(tests PRIVATE
    ../External/ImageCodec/Tests
    ../External/json/include
    ../OIViewer
    ../OIViewer/Source
    ../OIVAppCore/Source/Src
)
target_compile_definitions(tests PRIVATE
    "OIV_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    "OIV_IMAGE_CORPUS_DIR=\"${ExternalFolder}/ImageCodec/Tests\""
)
target_link_libraries(tests PRIVATE
    CLI11::CLI11
    Catch2::Catch2WithMain
    oivshared
    oivappcore
    RendererCore
    ImageCodec
    ImageUtil
    tiff
    LWSLib
)
if(TARGET libjpeg-turbo::libjpeg-turbo)
    target_link_libraries(tests PRIVATE libjpeg-turbo::libjpeg-turbo)
endif()
if(OIV_BUILD_RENDERER_D3D11)
    target_link_libraries(tests PRIVATE OIVD3D11Renderer)
endif()
if(OIV_BUILD_RENDERER_GL)
    target_link_libraries(tests PRIVATE OIVGLRenderer)
endif()
if(OIV_BUILD_RENDERER_VK)
    target_link_libraries(tests PRIVATE OIVVKRenderer)

    if(WIN32)
        add_executable(tests_vk_runtime_failure
            RendererIntegration/RuntimeFailureTests.cpp
            ../OIViewer/Source/CommandLine.cpp
            ../Renderers/OIVVKRenderer/Source/VKRuntimeWindows.cpp
        )
        target_compile_definitions(tests_vk_runtime_failure PRIVATE OIV_VK_TEST_MISSING_RUNTIME
            $<TARGET_PROPERTY:OIVVKRenderer,COMPILE_DEFINITIONS>)
        target_include_directories(tests_vk_runtime_failure PRIVATE
            ../OIViewer/Source ../OIVAppCore/Source/Src ../Renderers/OIVVKRenderer/Source)
        target_link_libraries(tests_vk_runtime_failure PRIVATE
            oivappcore CLI11::CLI11 Catch2::Catch2WithMain Vulkan::Headers user32)
    endif()
endif()

if(NOT OIV_BUILD_CLIENT)
    target_sources(tests PRIVATE
        ../OIViewer/Source/CommandLine.cpp
        ../OIViewer/Source/UI/MessageManager.cpp
        ../OIViewer/Source/LabelManager.cpp
        ../OIViewer/Source/ImageList.cpp
        ../OIViewer/Source/MouseGestureController.cpp
        ../OIViewer/Source/MouseMultiClickHandler.cpp
    )
endif()

if(WIN32)
    target_include_directories(tests PRIVATE ${ExternalFolder}/LInput/Include)
    if(NOT OIV_BUILD_CLIENT)
        target_sources(tests PRIVATE ../OIViewer/SrcPlatform/Win32/ContextMenuBackend.cpp)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(tests PRIVATE OIV_TEST_PLATFORM_LINUX)
    if(NOT TARGET PkgConfig::GTK3)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    endif()
    if(NOT TARGET PkgConfig::WaylandClient)
        pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
    endif()
    target_link_libraries(tests PRIVATE PkgConfig::GTK3 PkgConfig::WaylandClient)
    if(NOT OIV_BUILD_CLIENT)
        target_sources(tests PRIVATE
            ../OIViewer/SrcPlatform/Linux/ContextMenuBackend.cpp
            ../OIViewer/SrcPlatform/Linux/LinuxKeyBindings.cpp
        )
    endif()
endif()

if(OIV_BUILD_CLIENT)
    target_link_libraries(tests PRIVATE oivviewer)
    target_compile_definitions(tests PRIVATE OIV_TEST_VIEWER)
endif()
add_test(NAME oiv.integration COMMAND tests --reporter compact)

# FreeType and ImageCodec both use native PNG dependencies; keep their integration test isolated.
add_executable(tests_font_encoding FontEncoding/TestFontEncoding.cpp)
target_include_directories(tests_font_encoding PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_compile_definitions(tests_font_encoding PRIVATE
    "OIV_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
target_link_libraries(tests_font_encoding PRIVATE Catch2::Catch2WithMain FreeTypeWrapper LLUtils::LLUtils)
add_test(NAME oiv.font_encoding COMMAND tests_font_encoding --reporter compact)

foreach(testTarget IN ITEMS tests tests_font_encoding tests_imagecodec tests_vk_runtime_failure)
    if(TARGET ${testTarget})
        if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(${testTarget} PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
        elseif(MINGW)
            set_target_properties(${testTarget} PROPERTIES WIN32_EXECUTABLE FALSE)
            target_link_options(${testTarget} PRIVATE "-mconsole" "-municode")
        endif()
    endif()
endforeach()
