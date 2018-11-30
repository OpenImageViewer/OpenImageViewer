cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

set(PROJECT_NAME SolarSystem)
set(PROJECT_VERSION 1.0.0.0)
# Output Variables
set(OUTPUT_DEBUG Debug/bin)
set(OUTPUT_RELEASE Release/bin)
# Folders files
set(CPP_DIR_1 ./)
# set(CPP_DIR_2 ./)
set(CPP_DIR_3 API)
set(CPP_DIR_4 Commands)
set(CPP_DIR_5 Commands/Handlers)
set(CPP_DIR_6 External/easyexif)
set(CPP_DIR_7 FreeType)
# set(CPP_DIR_8 ./)
# set(CPP_DIR_9 ./)
set(HEADER_DIR_1 )
set(HEADER_DIR_2 API)
set(HEADER_DIR_3 Commands)
set(HEADER_DIR_4 Commands/Handlers)
set(HEADER_DIR_5 Common)
set(HEADER_DIR_6 External/easyexif)
set(HEADER_DIR_7 FreeType)
set(HEADER_DIR_8 Interfaces)

set(BASE_EXTERNAL_DIR ../Externals)
set(FREETYPE2_DIR ${BASE_EXTERNAL_DIR}/freetype2/libfreetype2-4.0.7/freetype2)
if(WIN32)
    set(GLEW_DIR ${BASE_EXTERNAL_DIR}/glew/glew-2.1.0-win32/glew-2.1.0/lib/Release)
    set(GLEW_LIB GLEW_DIR/Win32/glew32s.lib)
    # set(GLEW_LIB GLEW_DIR/x64/glew32s.lib)
elseif(UNIX)
    set(GLEW_DIR /usr/lib64/)
    set(GLEW_LIB libGLEW.a)
endif(WIN32)

set(D3D11 FALSE)
set(OPENGL TRUE)


# set(VERSION_MAJOR "2")
# set(VERSION_MINOR "6")
# set(VERSION_PATCH "5")

# set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
# set(SHARED_LIBRARY_VERSION ${VERSION_MAJOR}.${VERSION_MINOR})
############## CMake Project ################
#        The main options of project        #
#############################################

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} DESCRIPTION "Open Image Viewer" LANGUAGES CXX)

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
include_directories(${FREETYPE2_DIR}/include)

################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

option(BUILD_DEPENDS "Build other CMake project." ON)

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
if(BUILD_DEPENDS)
    add_subdirectory(../ImageCodec/ImageLoader ${CMAKE_BINARY_DIR}/ImageLoader)
    add_subdirectory(../ImageUtil ${CMAKE_BINARY_DIR}/ImageUtil)
    add_subdirectory(../Externals/freetype2/libfreetype2-4.0.7/freetype2 ${CMAKE_BINARY_DIR}/FreeType2) #TODO: Change to static lib prebuilt
    if(D3D11)
        add_subdirectory(platform/cmake/OIVD3D11Renderer ${CMAKE_BINARY_DIR}/OIVD3D11Renderer)
    endif(D3D11)
    # if(OPENGL)
    #     add_subdirectory(../Externals/glew/glew-2.1.0/build/cmake ${CMAKE_BINARY_DIR}/glew-2.1.0)
    # endif(OPENGL)
else()
    link_directories(dependencies/ImageLoader/build/)
    link_directories(dependencies/ImageUtil/build/)
    # if(D3D11)
    #     link_directories(dependencies/OIVD3D11Renderer/build/)
    # endif(D3D11)
    # if(OPENGL)
    #     link_directories(dependencies/OIVGLRenderer/build/)
    # endif(OPENGL)
endif(BUILD_DEPENDS)



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
    # ${CPP_DIR_2}/*.cpp
    ${CPP_DIR_3}/*.cpp
    ${CPP_DIR_4}/*.cpp
    ${CPP_DIR_5}/*.cpp
    ${CPP_DIR_6}/*.cpp
    ${CPP_DIR_7}/*.cpp
    # ${CPP_DIR_8}/*.cpp
    # ${CPP_DIR_9}/*.cpp
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
# target_link_libraries(${PROJECT_NAME} ImageLoader ImageUtil OIVD3D11Renderer )
# target_link_libraries(${PROJECT_NAME} ImageLoader ImageUtil freetype glew)
target_link_libraries(${PROJECT_NAME} ImageLoader ImageUtil freetype)

if(D3D11)
    link_directories(${D3D11_DIR})
    target_link_libraries(${PROJECT_NAME} ${D3D11_LIB})
endif(D3D11)
if(OPENGL)
    link_directories(${GLEW_DIR})
    target_link_libraries(${PROJECT_NAME} ${GLEW_DIR}/${GLEW_LIB})
endif(OPENGL)