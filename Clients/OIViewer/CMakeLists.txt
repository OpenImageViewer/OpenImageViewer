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

add_compile_definitions(GIT_HASH_ID="${GIT_HASH_ID}" GIT_COMMIT_DATE="${GIT_COMMIT_DATE}" GIT_BRANCH="${GIT_BRANCH}")

set(ExternalFolder ../../External)
set(TargetName OIViewer)
add_executable (${TargetName} ${sourceFiles})

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
     target_link_options(${TargetName} PRIVATE "/SUBSYSTEM:WINDOWS")
     # Don't automotically create Manifest, it is defined in the resource file
     target_link_options(${TargetName} PRIVATE "/MANIFEST:NO")
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
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/LLUtils/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/LInput/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/Win32/Win32/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageCodec/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/ImageCodec/ImageUtil/Include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/json/single_include)
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/xxHash)
target_include_directories(${TargetName} PRIVATE ../../oivlib/oiv/Include)


target_link_libraries(${TargetName}

#local libraries
ImageUtil
oiv
Win32
#system libraries
dbghelp.lib
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
     set_target_properties(${TargetName} PROPERTIES LINK_FLAGS
     "/DELAYLOAD:\"libpng16.dll\" /DELAYLOAD:\"FreeImage.dll\" /DELAYLOAD:\"D3DCompiler_47.dll\" /DELAYLOAD:\"turbojpeg.dll\" /DELAYLOAD:\"advapi32.dll\" /DELAYLOAD:\"SHELL32.dll\" /DELAYLOAD:\"MSIMG32.dll\" /DELAYLOAD:\"dbghelp.dll\" /DELAYLOAD:\"COMDLG32.dll\" /DELAYLOAD:\"Shcore.dll\" /DELAYLOAD:\"API-MS-WIN-SHCORE-SCALING-L1-1-1.DLL\"")
elseif(MINGW)
     message(WARNING "Delay load for MINGW is yet to be implemented")
elseif(CLANG)
     message(WARNING "Delay load for Clang is yet to be implemented")
endif()

#Link resources file to the executable
target_link_options(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/Resource.res)

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
