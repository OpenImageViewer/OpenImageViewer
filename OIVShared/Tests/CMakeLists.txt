file(GLOB testSources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Test*.cpp")

add_executable(tests_oivshared ${testSources})
target_link_libraries(tests_oivshared PRIVATE Catch2::Catch2WithMain oivshared)
add_test(NAME oivshared.unit COMMAND tests_oivshared --reporter compact)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_link_options(tests_oivshared PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
elseif(MINGW)
    set_target_properties(tests_oivshared PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_options(tests_oivshared PRIVATE "-mconsole" "-municode")
endif()

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_source_files_properties(TestUnitFormatter.cpp PROPERTIES COMPILE_OPTIONS "/fp:precise")
endif()
