#OIV library
cmake_minimum_required(VERSION 3.10)

#TODO: change source files to be under Source folder
file(GLOB_RECURSE sourceFiles
    "./*.h"
    "./*.cpp"
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
add_executable (${TargetName} WIN32 ${sourceFiles})

#Build resources for embedding an Icon into the executable


if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND NOT MINGW)
     add_custom_command(TARGET ${TargetName}
          PRE_LINK
          COMMAND ${CMAKE_RC_COMPILER}
          /d OIV_BUILD_TYPE=${OIV_BUILD_TYPE}
          /d OIV_OFFICIAL_BUILD=${OIV_OFFICIAL_BUILD}
          /fo "${CMAKE_CURRENT_BINARY_DIR}/Resource.res"
          "\"${CMAKE_CURRENT_SOURCE_DIR}/Resource.rc\""
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMENT "Compiling resources"
     )
     if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
          target_link_options(${TargetName} PRIVATE "/SUBSYSTEM:WINDOWS")
          # Don't automotically create Manifest, it is defined in the resource file
          target_link_options(${TargetName} PRIVATE "/MANIFEST:NO")
     else()
           
     endif()
     
else()
     add_custom_command(TARGET ${TargetName}
          PRE_LINK
          COMMAND ${CMAKE_RC_COMPILER}
          -D OIV_BUILD_TYPE=${OIV_BUILD_TYPE}
          -D OIV_OFFICIAL_BUILD=${OIV_OFFICIAL_BUILD}
          -O coff
          -o "${CMAKE_CURRENT_BINARY_DIR}/Resource.res"
          -i "\"${CMAKE_CURRENT_SOURCE_DIR}/Resource.rc\""
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMENT "Compiling resources"
     )
     if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
         target_link_options(${TargetName} PRIVATE "-mwindows")
     endif()
endif()
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/FreeTypeWrapper/FreeTypeWrapper/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ExoticNumbers/include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/LInput/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/Win32/Win32/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/single_include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_include_directories(${TargetName} PRIVATE ../../oivlib/oiv/Include)
target_include_directories(${TargetName} PRIVATE ../../oivlib/oiv/Source)


target_link_libraries(${TargetName}

#local libraries
LLUtils::LLUtils
ImageUtil
oiv
oivappcore
oivshared
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
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
elseif(MINGW)
     message(WARNING "Delay load for MINGW is yet to be implemented")
elseif(CLANG)
     message(WARNING "Delay load for Clang is yet to be implemented")
endif()

#Link resources file to the executable
# Currently when using Clang, Cmake adds to CMAKE_CXX_LINK_EXECUTABLE 'xlinker /Manifest:EMBDED' which cannot be overridden easily.
# That causes duplicate manifests, the custom we are adding here and the deafult one.
# A temporary fix — disable linking resources when using Clang on Windows.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
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
