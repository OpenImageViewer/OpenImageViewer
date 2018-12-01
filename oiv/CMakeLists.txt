cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

############## CMake Project ################
#        The main options of project        #
#############################################
set(PROJECT_NAME SolarSystem)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_TWEAK 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

message(STATUS "Building Version: ${PROJECT_VERSION}")
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} DESCRIPTION "Open Image Viewer" LANGUAGES CXX)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

# Output Variables
set(OUTPUT_DEBUG Debug/bin)
set(OUTPUT_RELEASE Release/bin)
# Folders files
set(CPP_DIR_1 ./)
set(CPP_DIR_3 API)
set(CPP_DIR_4 Commands)
set(CPP_DIR_5 Commands/Handlers)
set(CPP_DIR_6 External/easyexif)
set(CPP_DIR_7 FreeType)
set(HEADER_DIR_1 )
set(HEADER_DIR_2 API)
set(HEADER_DIR_3 Commands)
set(HEADER_DIR_4 Commands/Handlers)
set(HEADER_DIR_5 Common)
set(HEADER_DIR_6 External/easyexif)
set(HEADER_DIR_7 FreeType)
set(HEADER_DIR_8 Interfaces)

set(BASE_EXTERNAL_DIR ../Externals)

if(NOT CMAKE_GENERATOR_PLATFORM)
    message(STATUS "CMAKE_GENERATOR_PLATFORM has no value. Defaulting to x64.")
    set(X86 FALSE)
endif(NOT CMAKE_GENERATOR_PLATFORM)

set(FREETYPE2 TRUE)
set(FREETYPE2_BASE_DIR ${BASE_EXTERNAL_DIR}/freetype2/libfreetype2-4.0.7/freetype2)

set(D3D11 FALSE)
set(D3D11_BASE_DIR ${BASE_EXTERNAL_DIR}/D3D11_NOT_SET_YET)

set(OPENGL TRUE)
set(GLEW_BASE_DIR ${BASE_EXTERNAL_DIR}/glew/glew-2.1.0-win32/glew-2.1.0)

# set(SHARED_LIBRARY_VERSION ${VERSION_MAJOR}.${VERSION_MINOR})

# set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
# set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER API/api.h)
set_target_properties(${LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

# Definition of Macros
add_definitions(
   -DWIN32_LEAN_AND_MEAN 
   -DVC_EXTRALEAN 
   -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 
   -DUNICODE
   -D_UNICODE
)

add_compile_definitions(OIV_LIBRARY_BUILD=1)

############## Artefacts Output #################
# Defines outputs , depending Debug or Release. #
#################################################

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_DEBUG}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_DEBUG}")
  set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_DEBUG}")
else()
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_REL}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_REL}")
  set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_REL}")
endif()

# Include directories
include_directories(.)
# target_include_directories(${PROJECT_NAME} PRIVATE .)
include_directories(../ImageCodec/Image/Include)
include_directories(../ImageCodec/ImageLoader/Include)
include_directories(../ImageUtil/Include)
include_directories(../LLUtils/Include)

################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

option(BUILD_DEPENDS "Build other CMake project." ON)
option(BUILD_EXTERNAL_DEPENDS "Build 3rd party CMake projects." ON)

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
if(BUILD_DEPENDS)
    add_subdirectory(../ImageCodec/ImageLoader ${CMAKE_BINARY_DIR}/ImageLoader)
    add_subdirectory(../ImageUtil ${CMAKE_BINARY_DIR}/ImageUtil)
else()
    link_directories(dependencies/ImageLoader/build/)
    link_directories(dependencies/ImageUtil/build/)
endif(BUILD_DEPENDS)

if(BUILD_EXTERNAL_DEPENDS)
    if(FREETYPE2)
        set(FREETYPE2_LIB freetype)
        # include_directories(${FREETYPE2_DIR}/include)
        add_subdirectory(../Externals/freetype2/libfreetype2-4.0.7/freetype2 ${CMAKE_BINARY_DIR}/FreeType2)
    endif(FREETYPE2)
    if(D3D11)
        add_subdirectory(platform/cmake/OIVD3D11Renderer ${CMAKE_BINARY_DIR}/OIVD3D11Renderer)
        set(D3D11_LIB d3d11)
    endif(D3D11)
    if(OPENGL)
        set(OpenGL_GL_PREFERENCE "GLVND")
        set(GLEW_LIB glew)
        add_subdirectory(../Externals/glew/glew-2.1.0/build/cmake ${CMAKE_BINARY_DIR}/glew-2.1.0)
    endif(OPENGL)
else()
    if(FREETYPE2)
        if(WIN32)
            if(X86)
                set(FREETYPE2_DIR ${BASE_EXTERNAL_DIR}/freetype2/libfreetype2-4.0.7/freetype2/Win32)
                set(FREETYPE2_LIB ${FREETYPE2_DIR}/freetype2.lib)
            else()
                set(FREETYPE2_DIR ${BASE_EXTERNAL_DIR}/freetype2/libfreetype2-4.0.7/freetype2/x64)
                set(FREETYPE2_LIB ${FREETYPE2_DIR}/freetype2.lib)
            endif(X86)
        elseif(UNIX)
            set(FREETYPE2_DIR ${BASE_EXTERNAL_DIR}/freetype2/libfreetype2-4.0.7/freetype2)
            set(FREETYPE2_LIB freetype2.a)
        endif(WIN32)
        link_directories(${FREETYPE2_DIR})
    endif(FREETYPE2)
    if(D3D11)
        if(WIN32)
            set(D3D11_DIR ${D3D11_BASE_DIR}/D3D11_NOT_SET_YET)
            set(D3D11_LIB ${D3D11_DIR}/D3D11_NOT_SET_YET.lib)
        elseif(WIN64)
            set(D3D11_DIR ${D3D11_BASE_DIR}/D3D11_NOT_SET_YET/x64)
            set(D3D11_LIB ${D3D11_DIR}/D3D11_NOT_SET_YET.lib)
        elseif(UNIX)
            message(FATAL_ERROR "Direct3D is not supported on linux")
        endif(WIN32)
        link_directories(${D3D11_DIR})
    endif(D3D11)
    if(OPENGL)
        if(WIN32)
            if(X86)
                set(GLEW_DIR ${GLEW_BASE_DIR}/lib/Release/Win32)
                set(GLEW_LIB ${GLEW_DIR}/glew32s.lib)
            else()
                set(GLEW_DIR ${GLEW_BASE_DIR}/lib/Release/x64)
                set(GLEW_LIB ${GLEW_DIR}/glew32s.lib)
            endif()
        elseif(UNIX)
            set(GLEW_DIR /usr/lib64/)
            set(GLEW_LIB ${GLEW_DIR}/libGLEW.a)
        endif(WIN32)
        link_directories(${GLEW_DIR})
        # target_link_libraries(${PROJECT_NAME} ${GLEW_DIR}/${GLEW_LIB}) #TODO: DELETE
    endif(OPENGL)
endif(BUILD_EXTERNAL_DEPENDS)

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MD /Od /Zi /EHsc")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /GL /Zi /EHsc")
endif(MSVC)
if(NOT MSVC)
    if(ALL_WARNING)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif(ALL_WARNING)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif(NOT MSVC)

################ Files ################
#   --   Add files to project.   --   #
#######################################

file(GLOB SRC_FILES
    ${CPP_DIR_1}/*.cpp
    ${CPP_DIR_3}/*.cpp
    ${CPP_DIR_4}/*.cpp
    ${CPP_DIR_5}/*.cpp
    ${CPP_DIR_6}/*.cpp
    ${CPP_DIR_7}/*.cpp
    ${HEADER_DIR_1}/*.h
    ${HEADER_DIR_2}/*.h
    ${HEADER_DIR_3}/*.h
    ${HEADER_DIR_4}/*.h
    ${HEADER_DIR_5}/*.h
    ${HEADER_DIR_6}/*.h
    ${HEADER_DIR_7}/*.h
    ${HEADER_DIR_8}/*.h
)

# Add library to build.
add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

# Link with other dependencies.
target_link_libraries(${PROJECT_NAME} ImageLoader ImageUtil)

if(FREETYPE2)
    target_link_libraries(${PROJECT_NAME} ${FREETYPE2_LIB})
endif(FREETYPE2)
if(D3D11)
    target_link_libraries(${PROJECT_NAME} ${D3D11_LIB})
endif(D3D11)
if(OPENGL)
    target_link_libraries(${PROJECT_NAME} ${GLEW_LIB})
endif(OPENGL)

# MACROS
macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

# print_all_variables() # or debug