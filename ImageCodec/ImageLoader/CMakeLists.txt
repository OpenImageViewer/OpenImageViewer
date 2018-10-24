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
include_directories(./include)
include_directories(../../LLUtils/Include)
include_directories($ENV{solutiondir})
include_directories(../Image/Include)
include_directories(../)

################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

option(BUILD_DEPENDS 
   "Build other CMake project." 
   # ON 
   OFF #I added. was ON
)

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
if(BUILD_DEPENDS)
   add_subdirectory(platform/cmake/CodecDDS ${CMAKE_BINARY_DIR}/CodecDDS)
   add_subdirectory(platform/cmake/CodecFreeImage ${CMAKE_BINARY_DIR}/CodecFreeImage)
   add_subdirectory(platform/cmake/CodecGif ${CMAKE_BINARY_DIR}/CodecGif)
   add_subdirectory(platform/cmake/CodecJPG ${CMAKE_BINARY_DIR}/CodecJPG)
   add_subdirectory(platform/cmake/CodecPNG ${CMAKE_BINARY_DIR}/CodecPNG)
   add_subdirectory(platform/cmake/CodecPSD ${CMAKE_BINARY_DIR}/CodecPSD)
   add_subdirectory(platform/cmake/CodecTiff ${CMAKE_BINARY_DIR}/CodecTiff)
else(FALSE)#I added. was empty
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
target_link_libraries(${PROJECT_NAME} CodecDDS CodecFreeImage CodecGif CodecJPG CodecPNG CodecPSD CodecTiff )
