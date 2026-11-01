file(GLOB testSources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Test*.cpp")

add_executable(tests_oiviewer ${testSources})
target_include_directories(tests_oiviewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
    "${OIVIEWER_PLATFORM_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/single_include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/LInput/Include"
)
target_compile_definitions(tests_oiviewer PRIVATE
    "OIV_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}/Tests\"")
target_link_libraries(tests_oiviewer PRIVATE Catch2::Catch2WithMain)
if(TARGET oivviewer)
    target_link_libraries(tests_oiviewer PRIVATE oivviewer)
else()
    target_sources(tests_oiviewer PRIVATE
        ../Source/CommandLine.cpp
        ../Source/UI/MessageManager.cpp
        ../Source/LabelManager.cpp
        ../Source/ImageList.cpp
        ../Source/MouseGestureController.cpp
        ../Source/MouseMultiClickHandler.cpp
    )
    target_link_libraries(tests_oiviewer PRIVATE
        CLI11::CLI11
        oivappcore
        oivshared
        RendererCore
        ImageCodec
        ImageUtil
        LWSLib
    )
    if(WIN32)
        target_sources(tests_oiviewer PRIVATE ../SrcPlatform/Win32/ContextMenuBackend.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(tests_oiviewer PRIVATE
            ../SrcPlatform/Linux/ContextMenuBackend.cpp
            ../SrcPlatform/Linux/LinuxKeyBindings.cpp
        )
    endif()
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(tests_oiviewer PRIVATE OIV_TEST_PLATFORM_LINUX)
endif()
if(WIN32)
    target_sources(tests_oiviewer PRIVATE ../SrcPlatform/Win32/WlxPreview.cpp)
endif()
add_test(NAME oiviewer.unit COMMAND tests_oiviewer --reporter compact)

if(WIN32)
    add_executable(tests_file_associations
        Win32/FileAssociationTests.cpp
        ../SrcPlatform/Win32/FileAssociations.cpp
        ../SrcPlatform/Win32/FileAssociationDefaults.cpp
    )
    target_include_directories(tests_file_associations PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/single_include"
    )
    target_link_libraries(tests_file_associations PRIVATE
        Catch2::Catch2WithMain
        oivappcore
        WinReg::WinReg
        advapi32
        shlwapi
        shell32
        ole32
        bcrypt
        crypt32
    )
    add_test(NAME oiviewer.file_associations COMMAND tests_file_associations --reporter compact)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT TARGET PkgConfig::GTK3)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    endif()
    if(NOT TARGET PkgConfig::GIOUnix)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GIOUnix REQUIRED IMPORTED_TARGET gio-unix-2.0)
    endif()
    pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
    add_executable(tests_file_associations
        Linux/FileAssociationTests.cpp
        ../SrcPlatform/Linux/FileAssociations.cpp
    )
    target_include_directories(tests_file_associations PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/single_include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../OIVAppCore/Source/Include"
    )
    target_link_libraries(tests_file_associations PRIVATE
        Catch2::Catch2WithMain
        oivappcore
        PkgConfig::GIOUnix
    )
    add_test(NAME oiviewer.file_associations COMMAND tests_file_associations --reporter compact)

    add_library(tests_exception_dialog_probe SHARED Exception/LinuxDialogProbe.cpp)
    target_link_libraries(tests_exception_dialog_probe PRIVATE PkgConfig::GTK3 ${CMAKE_DL_LIBS})
    add_library(tests_association_dialog_probe SHARED Associations/LinuxDialogProbe.cpp)
    target_link_libraries(tests_association_dialog_probe PRIVATE PkgConfig::GTK3 PkgConfig::WaylandClient ${CMAKE_DL_LIBS})
endif()

if(WIN32)
    set(OIV_EXCEPTION_PLATFORM Win32)
else()
    set(OIV_EXCEPTION_PLATFORM Linux)
endif()
add_executable(tests_exception_process
    Exception/FailureProcess.cpp
    ../Source/ExceptionReporting.cpp
    ../SrcPlatform/${OIV_EXCEPTION_PLATFORM}/ExceptionHandler.cpp
)
target_include_directories(tests_exception_process PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/../SrcPlatform/${OIV_EXCEPTION_PLATFORM}"
)
target_link_libraries(tests_exception_process PRIVATE LLUtils::LLUtils)
if(WIN32)
    target_link_libraries(tests_exception_process PRIVATE user32 shell32 ole32)
else()
    target_link_libraries(tests_exception_process PRIVATE PkgConfig::GTK3)
endif()

foreach(testTarget IN ITEMS tests_oiviewer tests_file_associations)
    if(TARGET ${testTarget})
        if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(${testTarget} PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
        elseif(MINGW)
            set_target_properties(${testTarget} PROPERTIES WIN32_EXECUTABLE FALSE)
            target_link_options(${testTarget} PRIVATE "-mconsole" "-municode")
        endif()
    endif()
endforeach()
