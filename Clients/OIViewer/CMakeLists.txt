#OIV library
cmake_minimum_required(VERSION 3.11)

file(GLOB_RECURSE OIVIEWER_COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
     set(OIVIEWER_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/srcPlatform/Win32")
     include(FetchContent)
     FetchContent_Declare(
          winreg
          GIT_REPOSITORY https://github.com/GiovanniDicanio/WinReg.git
          GIT_TAG v6.4.0
     )
     FetchContent_GetProperties(winreg)
     if(NOT winreg_POPULATED)
          FetchContent_Populate(winreg)
     endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
     message(FATAL_ERROR "OIViewer Linux backend is not implemented yet. Configure with -DOIV_BUILD_CLIENT=OFF.")
else()
     message(FATAL_ERROR "Unsupported OIViewer platform: ${CMAKE_SYSTEM_NAME}")
endif()

file(GLOB_RECURSE OIVIEWER_PLATFORM_SOURCES
     "${OIVIEWER_PLATFORM_DIR}/*.h"
     "${OIVIEWER_PLATFORM_DIR}/*.hpp"
     "${OIVIEWER_PLATFORM_DIR}/*.cpp"
)

getGitHash(${CMAKE_CURRENT_SOURCE_DIR} GIT_HASH_ID)
getGitBranch(${CMAKE_CURRENT_SOURCE_DIR} GIT_BRANCH)
getGitCommitDate(${CMAKE_CURRENT_SOURCE_DIR} GIT_COMMIT_DATE)
getGitRevCount(${CMAKE_CURRENT_SOURCE_DIR} GIT_REV_COUNT)

add_compile_definitions(GIT_HASH_ID="${GIT_HASH_ID}"
GIT_COMMIT_DATE="${GIT_COMMIT_DATE}"
GIT_BRANCH="${GIT_BRANCH}"
OIV_VERSION_REVISION="${GIT_REV_COUNT}") 

set(ExternalFolder ../../External)
set(TargetName OIViewer)
add_executable(${TargetName})
target_sources(${TargetName} PRIVATE
     ${OIVIEWER_COMMON_SOURCES}
     ${OIVIEWER_PLATFORM_SOURCES}
)

#Build resources for embedding an Icon into the executable

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
     set_target_properties(${TargetName} PROPERTIES WIN32_EXECUTABLE TRUE)
     set(OIVIEWER_RESOURCE_FILE "${OIVIEWER_PLATFORM_DIR}/Resource.rc")
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
     if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
          target_link_options(${TargetName} PRIVATE "/SUBSYSTEM:WINDOWS")
          # Don't automotically create Manifest, it is defined in the resource file
          target_link_options(${TargetName} PRIVATE "/MANIFEST:NO")
     else()
           
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
     target_link_options(${TargetName} PRIVATE "-mwindows")
endif()
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${TargetName} PRIVATE ${OIVIEWER_PLATFORM_DIR})
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ExoticNumbers/include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/LInput/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/single_include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_include_directories(${TargetName} PRIVATE ../../oivlib/oiv/Include)
target_include_directories(${TargetName} PRIVATE ../../oivlib/oiv/Source)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
     target_include_directories(${TargetName} PRIVATE ${winreg_SOURCE_DIR}/WinReg)
     target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/Win32/Win32/Include)
endif()


target_link_libraries(${TargetName}

#local libraries
LLUtils::LLUtils
ImageUtil
oiv
oivappcore
oivshared
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
     target_link_libraries(${TargetName}
     #local libraries
     Win32
     #system libraries
     kernel32.lib
     user32.lib
     gdi32.lib
     shell32.lib
     advapi32.lib
     ole32.lib
     msimg32.lib
     comdlg32.lib
     shcore.lib
     hid
     )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
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

     if (LLUTILS_ENABLE_DEBUG_SYMBOLS)
          list(APPEND OIV_DELAYLOAD_DLLS "/DELAYLOAD:\"dbghelp.dll\"")
     endif()

     string(JOIN " " OIV_DELAYLOAD_LINK_FLAGS ${OIV_DELAYLOAD_DLLS})
     set_target_properties(${TargetName} PROPERTIES LINK_FLAGS "${OIV_DELAYLOAD_LINK_FLAGS}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MINGW)
     message(WARNING "Delay load for MINGW is yet to be implemented")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CLANG)
     message(WARNING "Delay load for Clang is yet to be implemented")
endif()

#Link resources file to the executable
# Currently when using Clang, Cmake adds to CMAKE_CXX_LINK_EXECUTABLE 'xlinker /Manifest:EMBDED' which cannot be overridden easily.
# That causes duplicate manifests, the custom we are adding here and the deafult one.
# A temporary fix — disable linking resources when using Clang on Windows.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
     target_link_options(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/Resource.res)
endif()

#Copy own resources to the output dir
add_custom_command(TARGET ${TargetName} POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Resources ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/Resources)
#Copy oiv library resources to the output dir
add_custom_command(TARGET ${TargetName} POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../../oivlib/oiv/Resources ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/Resources)

if (NOT MINGW_POST_BUILD_DONT_COPY_BINARIES AND MINGW AND CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
     set(MINGW_ROOT /usr/x86_64-w64-mingw32)
     add_custom_command(TARGET ${TargetName} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_ROOT}/lib/libgcc_s_seh-1.dll ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/)

     add_custom_command(TARGET ${TargetName} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_ROOT}/lib/libstdc++-6.dll ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/)

     add_custom_command(TARGET ${TargetName} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_ROOT}/bin/libwinpthread-1.dll ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin/)
endif()     
