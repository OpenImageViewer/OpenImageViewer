cmake_minimum_required(VERSION 3.5)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

set(PROJECT_NAME OIVGLRenderer)
# Output Variables
set(OUTPUT_DEBUG Debug/bin)
set(OUTPUT_RELEASE Release/bin)
# Folders files
set(CPP_DIR_1 ./)
set(CPP_DIR_2 ./)
set(CPP_DIR_3 ./)
set(HEADER_DIR_1 )
set(HEADER_DIR_2 GLRenderer)
set(HEADER_DIR_3 GLRenderer/Win32)

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
   -DNOMINMAX 
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
include_directories(E:/Downloads/glew-2.0.0/include)
include_directories(../LLUtils/Include)
include_directories(../ImageCodec/Image/Include)

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

if(MSVC)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MD /Od /EHsc")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /GL /EHsc")
endif(MSVC)
if(NOT MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
   if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   endif()
endif(NOT MSVC)

################ Files ################
#   --   Add files to project.   --   #
#######################################

#file(GLOB SRC_FILES
#    ${CPP_DIR_1}/*.cpp
#    ${CPP_DIR_2}/*.cpp
#    ${CPP_DIR_3}/*.cpp
#    ${HEADER_DIR_1}/*.h
#    ${HEADER_DIR_2}/*.h
#    ${HEADER_DIR_3}/*.h
#)
#
## Add library to build.
#add_library(${PROJECT_NAME} STATIC
#   ${SRC_FILES}
#)
#
#if(MSVC)
##   target_link_libraries(${PROJECT_NAME} E:\Downloads\glew-2.0.0-win32\glew-2.0.0\lib\Release\x64\glew32s.lib )
#endif(MSVC)
