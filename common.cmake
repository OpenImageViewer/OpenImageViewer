
function (setCommonCompileParameters)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        #for MSVC and clang-cl 32 bit target
        if (CMAKE_SIZEOF_VOID_P EQUAL 4)
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/LARGEADDRESSAWARE>)
        endif()
    #use premissive code and opimized floating point model for MSVC and clang-cl 
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->) # Confrom to standards.
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/fp:fast>) # Enable non-standard optimized floating point model.
    endif()
    #Disable crt secure, and minmax warnings
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()
endfunction()

function (getGitShortHash working_dir output)
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${working_dir}
        OUTPUT_VARIABLE value
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result
    )
    if (NOT result EQUAL 0 OR value STREQUAL "")
        message(FATAL_ERROR "Failed to resolve the Git short hash in ${working_dir}.")
    endif()
    set(${output} ${value} PARENT_SCOPE)
endfunction()

function (getGitRevision working_dir output)
    execute_process(
        COMMAND git rev-list HEAD --count
        WORKING_DIRECTORY ${working_dir}
        OUTPUT_VARIABLE value
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result
    )
    if (NOT result EQUAL 0 OR NOT value MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "Failed to resolve the Git revision in ${working_dir}. "
            "Configure with -DOIV_VERSION_REVISION=<revision> when Git history is unavailable."
        )
    endif()
    set(${output} ${value} PARENT_SCOPE)
endfunction()

