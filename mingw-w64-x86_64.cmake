
#Usage: cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-x86_64.cmake

set(CMAKE_SYSTEM_NAME Windows)


set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# cross compilers to use for C, C++ and Fortran
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 10)


function (createSymLink target symlink)
    cmake_path(GET symlink PARENT_PATH parentPath)
    file(MAKE_DIRECTORY ${parentPath})
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${target} ${symlink})
endfunction()

# MINGW defaults max SDK version to Windows server 2003 (0x0502) 
# For supporting newer functionality, overriding is needed, otherwise code won't compile
# override with Windows 10 (0x0A00) SDK version
add_compile_definitions(_WIN32_WINNT=0x0A00)

set(CASE_SENSITIVE_FIX_FOR_MINGW_PATH ${CMAKE_CURRENT_BINARY_DIR}/mingw-w64)
createSymLink(/usr/x86_64-w64-mingw32/include/windows.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/Windows.h)
createSymLink(/usr/x86_64-w64-mingw32/include/shlobj.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/ShlObj.h)
createSymLink(/usr/x86_64-w64-mingw32/include/dbghelp.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/DbgHelp.h)
createSymLink(/usr/x86_64-w64-mingw32/include/commctrl.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/CommCtrl.h)
createSymLink(/usr/x86_64-w64-mingw32/include/shellscalingapi.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/ShellScalingApi.h)
createSymLink(/usr/x86_64-w64-mingw32/include/psapi.h ${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include/Psapi.h)

include_directories(${CASE_SENSITIVE_FIX_FOR_MINGW_PATH}/Include)
