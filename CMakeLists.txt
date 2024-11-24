#root CMake file
cmake_minimum_required(VERSION 3.8)

project(OpenImageViewer)

if (${CMAKE_CORSSCOMPILING})
    message("Cross compiling: Host: ${CMAKE_HOST_SYSTEM_NAME}, Target: ${CMAKE_SYSTEM_NAME}")
endif()

if (MINGW)
    #temporary compilation fix -  disable firibi and webp
    option(FREETYPE_WRAPPER_BUILD_FRIBIDI "build fribidi" OFF)
    option(IMCODEC_BUILD_CODEC_WEBP "Build Codec WEBP" FALSE)
endif()

include (common.cmake)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

option(OIV_OFFICIAL_BUILD "Create official binary of OIV" FALSE)
option(OIV_OFFICIAL_RELEASE "Build official release of OIV" FALSE)
option(OIV_WARNING_LEVEL_EXTREME "Warning level extreme" FALSE)
option(OIV_DISABLE_WARNINGS_EXTERNAL_LIBS "Disable warnings for external libraries" TRUE)
option(OIV_VERBOSE "Verbose" FALSE)

# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

message("Build type: " ${CMAKE_BUILD_TYPE})

if (OIV_VERBOSE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-v)
    endif()
endif()


if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(OIV_BUILD_TYPE 0)
else()
    set(OIV_BUILD_TYPE 1)
endif()


if (OIV_OFFICIAL_BUILD)
    set(OIV_OFFICIAL_BUILD 1)
else()
    set(OIV_OFFICIAL_BUILD 0)
endif()

add_compile_definitions(OIV_OFFICIAL_BUILD=${OIV_OFFICIAL_BUILD})

if (OIV_OFFICIAL_RELEASE)
    set(OIV_OFFICIAL_RELEASE 1)
else()
    set(OIV_OFFICIAL_RELEASE 0)
endif()

add_compile_definitions(OIV_OFFICIAL_RELEASE=${OIV_OFFICIAL_RELEASE})

if (OIV_VERSION_BUILD)
    add_compile_definitions(OIV_VERSION_BUILD=${OIV_VERSION_BUILD})
endif()

if (OIV_RELEASE_SUFFIX)
  add_compile_definitions(OIV_RELEASE_SUFFIX=${OIV_RELEASE_SUFFIX})
endif()

#Optimizations
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        #MSVC optimizations
        #compiler optimizations
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/O2>)    #maximum optimization
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Ob2>)   #inline function expansion
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Oi>)    #enable instrinsic functions
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Ot>)    #favor fast code
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Oy>)    #Omit frame pointers
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/GT>)    #enalbe fiber safe optimiations
    
        #linker optimizations
        add_link_options(/OPT:REF)  #remove unreferenced code/data
        add_link_options(/OPT:ICF)  #enable COMDAT folding
    
        #   Publish specific optimization flags
        #if (OIV_OFFICIAL_BUILD)
           add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/GL>)    #whole program optimization
           add_link_options(/LTCG)     #link time optimization code generation
        #endif()

        #Always generate debug info
        add_link_options(/DEBUG)    #(Generate Debug Info)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zo>)    #Enhance Optimized Debugging
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zi>)    #seperate debug information
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-flto>) #link time optimization code generation
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fwhole-program-vtables>)
        # fstrict-vtable-pointers and fstrict-enums don't seems to be supported in clang-cl 16
        if (${CMAKE_CXX_COMPILER_VERSION} GREATER_EQUAL "16" AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-vtable-pointers>)    
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>)
        endif()
    endif()
endif()





#Warnings
if (OIV_WARNING_LEVEL_EXTREME)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    #enabled warnings flags
        add_compile_options(-Wextra -Wpedantic -Wall -Wpessimizing-move)
        #specifically disable the following warning flags
        add_compile_options(-Wno-c++98-compat -Wno-c++11-compat -Wno-c++14-compat -Wno-c++98-compat-pedantic -Wno-reserved-id-macro -Wno-newline-eof -Wno-global-constructors -Wno-exit-time-destructors -Wno-float-equal -Wno-covered-switch-default -Wno-unreachable-code-break)
      # using Clang
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      # using GCC
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
      # using Intel C++
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        add_compile_options(/permissive-)
        #add_compile_options(/W1)
    # using Visual Studio C++
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/Wall /wd4668 /wd5039 /wd4514 /wd4820 /wd4625 /wd4626 /wd4710 /wd5045 /wd4061" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
      else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
      endif()
    endif()
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
setCommonCompileParameters()
add_compile_definitions(UNICODE _UNICODE)
add_definitions(-DLLUTILS_ENABLE_DEBUG_SYMBOLS=1)
include_directories(./External/LLUtils/Include)
include_directories(./Extensions)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    option(WIN32_LIB_BUILD_SAMPLES "build win32 samples" OFF)
    add_subdirectory(./External/Win32)
endif()
add_subdirectory(oivlib)
add_subdirectory(Clients/OIViewer)
