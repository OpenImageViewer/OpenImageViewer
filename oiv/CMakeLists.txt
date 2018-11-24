cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

set(PROJECT_NAME SolarSystem)
# Output Variables
set(OUTPUT_DEBUG Debug/bin)
set(OUTPUT_RELEASE Release/bin)
# Folders files
set(CPP_DIR_1 ./)
set(CPP_DIR_2 ./)
set(CPP_DIR_3 API)
set(CPP_DIR_4 Commands)
set(CPP_DIR_5 Commands/Handlers)
set(CPP_DIR_6 External/easyexif)
set(CPP_DIR_7 FreeType)
set(CPP_DIR_8 ./)
set(CPP_DIR_9 ./)
set(HEADER_DIR_1 )
set(HEADER_DIR_2 API)
set(HEADER_DIR_3 Commands)
set(HEADER_DIR_4 Commands/Handlers)
set(HEADER_DIR_5 Common)
set(HEADER_DIR_6 External/easyexif)
set(HEADER_DIR_7 FreeType)
set(HEADER_DIR_8 Interfaces)
set(FREETYPE2_DIR ../Externals/libfreetype2-4.0.7/freetype2)

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
   -DOIV_NO_CLIENT_BUILD 
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
# include_directories($ENV{SolutionDir}/LLUtils/Include)
include_directories(../ImageCodec/Image/Include)
include_directories(../ImageCodec/ImageLoader/Include)
include_directories(../ImageUtil/Include)
include_directories(${FREETYPE2_DIR}/include)
# include_directories(E:/Downloads/freetype-2.3.5-1-bin/include/freetype2/)
include_directories(../LLUtils/Include)

################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

option(BUILD_DEPENDS 
   "Build other CMake project." 
   #ON 
   OFF #TODO: Change to ON
)

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
if(BUILD_DEPENDS)
   add_subdirectory(platform/cmake/ImageLoader ${CMAKE_BINARY_DIR}/ImageLoader)
   add_subdirectory(platform/cmake/ImageUtil ${CMAKE_BINARY_DIR}/ImageUtil)
   add_subdirectory(platform/cmake/OIVD3D11Renderer ${CMAKE_BINARY_DIR}/OIVD3D11Renderer)
else()
   link_directories(dependencies/ImageLoader/build/)
   link_directories(dependencies/ImageUtil/build/)
   link_directories(dependencies/OIVD3D11Renderer/build/)
endif()

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

if(MSVC)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MD /Od /Zi /EHsc")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /GL /Zi /EHsc")
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

file(GLOB SRC_FILES
    ${CPP_DIR_1}/*.cpp
    ${CPP_DIR_2}/*.cpp
    ${CPP_DIR_3}/*.cpp
    ${CPP_DIR_4}/*.cpp
    ${CPP_DIR_5}/*.cpp
    ${CPP_DIR_6}/*.cpp
    ${CPP_DIR_7}/*.cpp
    ${CPP_DIR_8}/*.cpp
    ${CPP_DIR_9}/*.cpp
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
add_library(${PROJECT_NAME} SHARED
   ${SRC_FILES}
)

# Link with other dependencies.
target_link_libraries(${PROJECT_NAME} ImageLoader ImageUtil OIVD3D11Renderer )
if(MSVC)
   target_link_libraries(${PROJECT_NAME} E:\Downloads\glew-2.0.0-win32\glew-2.0.0\lib\Release\Win32\glew32s.lib )
endif(MSVC)
