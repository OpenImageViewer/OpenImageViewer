cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

if(NOT EXISTS "${PLUGIN_FILE}")
    message(FATAL_ERROR "Build the WLX frontend before packaging")
endif()
get_filename_component(binaryDirectory "${PLUGIN_FILE}" DIRECTORY)
get_filename_component(pluginName "${PLUGIN_FILE}" NAME)
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${PLUGIN_FILE}"
    DIRECTORIES "${binaryDirectory}"
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\].*"
    RESOLVED_DEPENDENCIES_VAR dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved)
if(unresolved)
    message(FATAL_ERROR "Unresolved WLX dependencies: ${unresolved}")
endif()
set(runtimeDlls)
foreach(dependency IN LISTS dependencies)
    get_filename_component(name "${dependency}" NAME)
    file(COPY_FILE "${dependency}" "${binaryDirectory}/${name}" ONLY_IF_DIFFERENT)
    list(APPEND runtimeDlls "${name}")
endforeach()
set(packageFiles "${pluginName}" ${runtimeDlls} Resources WLX.md LICENSE.md pluginst.inf)
if(EXISTS "${binaryDirectory}/ShaderCache")
    list(APPEND packageFiles ShaderCache)
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT_FILE}" --format=zip ${packageFiles}
    WORKING_DIRECTORY "${binaryDirectory}"
    COMMAND_ERROR_IS_FATAL ANY
)
message(STATUS "WLX package: ${OUTPUT_FILE}")
