# OIViewer client
cmake_minimum_required(VERSION 3.20)

if(OIV_BUILD_WLX)
    set(TargetName OIViewerWlx)
else()
    set(TargetName OIViewer)
endif()
set(ExternalFolder ../../External)

if(OIV_BUILD_WLX)
    add_library(${TargetName} SHARED)
    set_target_properties(${TargetName} PROPERTIES PREFIX "" OUTPUT_NAME OIViewer SUFFIX ".wlx64")
else()
    add_executable(${TargetName})
endif()

file(GLOB_RECURSE OIVIEWER_SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
)

# Reusable UI implementation is compiled once and linked by the selected frontend and tests.
# Entry points and shell composition remain outside this target.
list(FILTER OIVIEWER_SOURCE_FILES EXCLUDE REGEX "/(Main|ViewerApplication[^/]*)[.]cpp$")
add_library(oivviewer STATIC ${OIVIEWER_SOURCE_FILES})
set(FrontendTarget ${TargetName})
set(TargetName oivviewer)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(OIVIEWER_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SrcPlatform/Win32")

    add_subdirectory(SrcPlatform/Win32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OIVIEWER_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SrcPlatform/Linux")
    add_subdirectory(SrcPlatform/Linux)
else()
    message(FATAL_ERROR "Unsupported OIViewer platform: ${CMAKE_SYSTEM_NAME}")
endif()

set(TargetName ${FrontendTarget})
if(NOT OIV_BUILD_WLX)
    file(GLOB OIV_SHELL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/ViewerApplication*.cpp"
        "${OIVIEWER_PLATFORM_DIR}/ViewerApplication*.cpp")
    target_sources(${TargetName} PRIVATE Source/Main.cpp ${OIV_SHELL_SOURCES})
endif()

if(OIV_GIT_SHORT_HASH STREQUAL "")
    getGitShortHash(${CMAKE_CURRENT_SOURCE_DIR} OIV_GIT_SHORT_HASH)
endif()
string(LENGTH "${OIV_GIT_SHORT_HASH}" OIV_GIT_SHORT_HASH_LENGTH)
if(NOT OIV_GIT_SHORT_HASH MATCHES "^[0-9a-f]+$" OR OIV_GIT_SHORT_HASH_LENGTH LESS 8)
    message(FATAL_ERROR
        "OIV_GIT_SHORT_HASH must contain at least eight lowercase hexadecimal characters, "
        "got '${OIV_GIT_SHORT_HASH}'."
    )
endif()
target_compile_definitions(oivviewer PUBLIC OIV_GIT_SHORT_HASH="${OIV_GIT_SHORT_HASH}")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # The console subsystem makes shells wait. The embedded detached-console policy
    # prevents Explorer launches from allocating a console on Windows 11 24H2 and newer.
    set_target_properties(${TargetName} PROPERTIES WIN32_EXECUTABLE FALSE)
    set(OIVIEWER_RESOURCE_FILE "${OIVIEWER_PLATFORM_DIR}/Resource.rc")
    set_property(TARGET ${TargetName} APPEND PROPERTY LINK_DEPENDS
        "${OIVIEWER_RESOURCE_FILE}" "${OIVIEWER_PLATFORM_DIR}/OIViewerManifest.xml")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT MINGW)
    add_custom_command(TARGET ${TargetName}
        PRE_LINK
        COMMAND ${CMAKE_RC_COMPILER}
        /d OIV_BUILD_TYPE=${OIV_BUILD_TYPE}
        /d OIV_OFFICIAL_BUILD=${OIV_OFFICIAL_BUILD}
        /i "${OIVIEWER_PLATFORM_DIR}"
        /i "${CMAKE_CURRENT_SOURCE_DIR}"
        /fo "${CMAKE_CURRENT_BINARY_DIR}/Resource.res"
        "\"${OIVIEWER_RESOURCE_FILE}\""
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Compiling resources"
    )
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_link_options(${TargetName} PRIVATE "/MANIFEST:NO")
        if(NOT OIV_BUILD_WLX)
            target_link_options(${TargetName} PRIVATE "/ENTRY:wmainCRTStartup")
        endif()
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(TARGET ${TargetName}
        PRE_LINK
        COMMAND ${CMAKE_RC_COMPILER}
        -D OIV_BUILD_TYPE=${OIV_BUILD_TYPE}
        -D OIV_OFFICIAL_BUILD=${OIV_OFFICIAL_BUILD}
        -I "${OIVIEWER_PLATFORM_DIR}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}"
        -O coff
        -o "${CMAKE_CURRENT_BINARY_DIR}/Resource.res"
        -i "\"${OIVIEWER_RESOURCE_FILE}\""
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Compiling resources"
    )
    if(NOT OIV_BUILD_WLX)
        target_link_options(${TargetName} PRIVATE "-mconsole" "-municode")
    endif()
endif()

target_include_directories(oivviewer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${OIVIEWER_PLATFORM_DIR}
    ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include
    ${ExternalFolder}/ExoticNumbers/include
    ${ExternalFolder}/LInput/Include
    ${ExternalFolder}/ImageCodec/ImageCodec/Include
    ${ExternalFolder}/ImageCodec/ImageUtil/Include
    ${ExternalFolder}/json/single_include
    ${ExternalFolder}/xxHash
)


target_link_libraries(oivviewer PUBLIC
    CLI11::CLI11
    LLUtils::LLUtils
    ImageUtil
    oiv
    oivappcore
    oivshared
    LWSLib
)

target_link_libraries(${TargetName} PRIVATE oivviewer)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(oivviewer PUBLIC
        kernel32.lib
        user32.lib
        gdi32.lib
        shell32.lib
        WinReg::WinReg
        advapi32.lib
        ole32.lib
        msimg32.lib
        comdlg32.lib
        comctl32.lib
        shlwapi.lib
        bcrypt.lib
        crypt32.lib
        shcore.lib
        hid
    )
endif()

if(NOT OIV_BUILD_WLX AND CMAKE_SYSTEM_NAME STREQUAL "Windows" AND
   (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    set(OIV_DELAYLOAD_DLLS
        "/DELAYLOAD:\"libpng16.dll\""
        "/DELAYLOAD:\"FreeImage.dll\""
        "/DELAYLOAD:\"D3DCompiler_47.dll\""
        "/DELAYLOAD:\"turbojpeg.dll\""
        "/DELAYLOAD:\"advapi32.dll\""
        "/DELAYLOAD:\"SHELL32.dll\""
        "/DELAYLOAD:\"MSIMG32.dll\""
        "/DELAYLOAD:\"COMDLG32.dll\""
        "/DELAYLOAD:\"Shcore.dll\""
        "/DELAYLOAD:\"API-MS-WIN-SHCORE-SCALING-L1-1-1.DLL\"")

    if(LLUTILS_ENABLE_DEBUG_SYMBOLS)
        list(APPEND OIV_DELAYLOAD_DLLS "/DELAYLOAD:\"dbghelp.dll\"")
    endif()

    string(JOIN " " OIV_DELAYLOAD_LINK_FLAGS ${OIV_DELAYLOAD_DLLS})
    set_target_properties(${TargetName} PROPERTIES LINK_FLAGS "${OIV_DELAYLOAD_LINK_FLAGS}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MINGW)
    message(WARNING "Delay load for MINGW is yet to be implemented")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CLANG)
    message(WARNING "Delay load for Clang is yet to be implemented")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
    target_link_options(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/Resource.res)
endif()

add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:${TargetName}>/Resources)
add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../../OIVLib/OIV/Resources $<TARGET_FILE_DIR:${TargetName}>/Resources)

if(NOT MINGW_POST_BUILD_DONT_COPY_BINARIES AND MINGW AND CMAKE_CROSSCOMPILING AND
   CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(MINGW_ROOT /usr/x86_64-w64-mingw32)
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MINGW_ROOT}/lib/libgcc_s_seh-1.dll $<TARGET_FILE_DIR:${TargetName}>/)
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MINGW_ROOT}/lib/libstdc++-6.dll $<TARGET_FILE_DIR:${TargetName}>/)
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MINGW_ROOT}/bin/libwinpthread-1.dll $<TARGET_FILE_DIR:${TargetName}>/)
endif()

if(OIV_BUILD_WLX)
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/WLX.md" "$<TARGET_FILE_DIR:${TargetName}>/WLX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/pluginst.inf" "$<TARGET_FILE_DIR:${TargetName}>/pluginst.inf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE.md" "$<TARGET_FILE_DIR:${TargetName}>/LICENSE.md")
    add_custom_target(package_wlx
        COMMAND ${CMAKE_COMMAND}
            "-DPLUGIN_FILE=$<TARGET_FILE:${TargetName}>"
            "-DOUTPUT_FILE=${CMAKE_BINARY_DIR}/OIViewer-wlx-x64.zip"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/PackageWlx.cmake"
        DEPENDS ${TargetName}
        VERBATIM)
endif()
