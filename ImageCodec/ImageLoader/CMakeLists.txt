cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

set(PROJECT_NAME ImageLoader)
# Output Variables
set(OUTPUT_DEBUG Debug/bin)
set(OUTPUT_RELEASE Release/bin)
# Folders files
set(CPP_DIR_1 Source)
set(HEADER_DIR_1 ../Image/Include)
set(HEADER_DIR_2 Include)
set(HEADER_DIR_3 Source)
set(OIV_BUILD_ARCHITRCTURE "x64")

############## CMake Project ################
#        The main options of project        #
#############################################

project(${PROJECT_NAME} CXX)

# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

# Definition of Macros
add_definitions(
   -D_DEBUG 
   -D_LIB 
   -DWIN32_LEAN_AND_MEAN 
   -DVC_EXTRALEAN 
   -DUNICODE
   -D_UNICODE
)

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
#include_directories(./include)
# include_directories(../../LLUtils/Include)
# include_directories($ENV{solutiondir})
# include_directories(../Image/Include)
# include_directories(../)
if(OIV_BUILD_ARCHITRCTURE STREQUAL "x86")
    include_directories(../../Externals/FreeImage/FreeImage3180Win32Win64/FreeImage/Dist/x32)
elseif(OIV_BUILD_ARCHITRCTURE STREQUAL "x64")
    include_directories(../../Externals/FreeImage/FreeImage3180Win32Win64/FreeImage/Dist/x64)
else()
    message(FATAL_ERROR "Print error to screen")
endif()
################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

option(BUILD_DEPENDS "Build other CMake project." ON)
option(DDS "Build DDS Codec." OFF)
option(FREE_IMAGE "Build FREE_IMAGE Codec" OFF)
option(GIF "Build GIF Codec" OFF)
option(JPG "Build JPG Codec" OFF)
option(PNG "Build FREE_IMAGE Codec" OFF)
option(PSD "Build FREE_IMAGE Codec" OFF)
option(TIFF "Build FREE_IMAGE Codec" OFF)

# This is here to write a file for the configuration
# file(WRITE version.cpp
#      "const char* getVersion() { return \"${MyProj_VERSION}\"; }"
#  )

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
if(BUILD_DEPENDS)
    if(DDS)
        add_subdirectory(../Codecs/CodecDDS ${CMAKE_BINARY_DIR}/CodecDDS)
        add_compile_definitions(IMCODEC_BUILD_CODEC_DDS=1)
        target_link_libraries(${PROJECT_NAME} CodecDDS)
    endif()
    if(FREE_IMAGE)
        add_subdirectory(../Codecs/CodecFreeImage ${CMAKE_BINARY_DIR}/CodecFreeImage)
        add_compile_definitions(IMCODEC_BUILD_CODEC_FREE_IMAGE=1)
        target_link_libraries(${PROJECT_NAME} CodecFreeImage)
    endif()
    if(GIF)
        add_subdirectory(../Codecs/CodecGif ${CMAKE_BINARY_DIR}/CodecGif)
        add_compile_definitions(IMCODEC_BUILD_CODEC_GIF=1)
        target_link_libraries(${PROJECT_NAME} CodecGif)
    endif()
    if(JPG)
        add_subdirectory(../Codecs/CodecJPG ${CMAKE_BINARY_DIR}/CodecJPG)
        add_compile_definitions(IMCODEC_BUILD_CODEC_JPG=1)
        target_link_libraries(${PROJECT_NAME} CodecJPG)
    endif()
    if(PNG)
        add_subdirectory(../Codecs/CodecPNG ${CMAKE_BINARY_DIR}/CodecPNG)
        add_compile_definitions(IMCODEC_BUILD_CODEC_PNG=1)
        target_link_libraries(${PROJECT_NAME} CodecPNG)
    endif()
    if(PSD)
        add_subdirectory(../Codecs/CodecPSD ${CMAKE_BINARY_DIR}/CodecPSD)
        add_compile_definitions(IMCODEC_BUILD_CODEC_PSD=1)
        target_link_libraries(${PROJECT_NAME} CodecPSD)
    endif()
    if(TIFF)
        add_subdirectory(../Codecs/CodecTiff ${CMAKE_BINARY_DIR}/CodecTiff)
        add_compile_definitions(IMCODEC_BUILD_CODEC_TIFF=1)
        target_link_libraries(${PROJECT_NAME} CodecTiff)
    endif()
else()
   link_directories(dependencies/CodecDDS/build/)
   link_directories(dependencies/CodecFreeImage/build/)
   link_directories(dependencies/CodecGif/build/)
   link_directories(dependencies/CodecJPG/build/)
   link_directories(dependencies/CodecPNG/build/)
   link_directories(dependencies/CodecPSD/build/)
   link_directories(dependencies/CodecTiff/build/)
endif()

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

if(MSVC)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MD /Od /EHsc")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /GL /EHsc")
endif(MSVC)
if(NOT MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -lstdc++fs")
   if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   endif()
endif(NOT MSVC)

################ Files ################
#   --   Add files to project.   --   #
#######################################

file(GLOB SRC_FILES
    ${CPP_DIR_1}/*.cpp
    ${HEADER_DIR_1}/*.h
    ${HEADER_DIR_2}/*.h
    ${HEADER_DIR_3}/*.h
)

# Add library to build.
add_library(${PROJECT_NAME} STATIC
   ${SRC_FILES}
)

# Link with other dependencies.
#target_link_libraries(${PROJECT_NAME} CodecDDS CodecFreeImage CodecGif CodecJPG CodecPNG CodecPSD CodecTiff )
