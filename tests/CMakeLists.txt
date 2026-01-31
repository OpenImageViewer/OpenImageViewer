
file(GLOB_RECURSE sourceFiles
    "./source/*.h"
    "./source/*.cpp"
)

set(ExternalFolder "${CMAKE_CURRENT_SOURCE_DIR}/../External")
cmake_path(ABSOLUTE_PATH ExternalFolder NORMALIZE OUTPUT_VARIABLE ExternalFolder)

set(TargetName tests)

add_subdirectory(${ExternalFolder}/catch2 ./external/catch2)

add_executable (${TargetName} ${sourceFiles})
target_include_directories(${TargetName} PRIVATE ${ExternalFolder}/catch2/src)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Clients/OIViewer)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../oivlib/oiv/Include)
target_link_libraries(${TargetName} PRIVATE Catch2::Catch2WithMain oivshared ImageCodec)

if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_link_options(${TargetName} PRIVATE "/SUBSYSTEM:CONSOLE")
    target_link_options(${TargetName} PRIVATE "/ENTRY:wmainCRTStartup")
endif()
           
