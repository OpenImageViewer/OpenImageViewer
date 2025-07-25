
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

function (getGitHash working_dir hash)
    execute_process (COMMAND git rev-parse --short=8 HEAD WORKING_DIRECTORY ${working_dir} OUTPUT_VARIABLE hash_temp)
    string(REPLACE \n "" hash_temp ${hash_temp})
    SET(${hash} ${hash_temp} PARENT_SCOPE)
endfunction()


function (getGitBranch working_dir branchName)
    execute_process (COMMAND git branch "--format=%(refname:short)" WORKING_DIRECTORY ${working_dir} OUTPUT_VARIABLE temp)
    string(REPLACE \n "" temp ${temp})
    SET(${branchName} ${temp} PARENT_SCOPE)
endfunction()

function (getGitCommitDate working_dir commitDate)
    execute_process (COMMAND git log -1 "--date=format:%Y-%m-%d %H:%M:%S" --format=%cd WORKING_DIRECTORY ${working_dir} OUTPUT_VARIABLE temp)
    string(REPLACE \n "" temp ${temp})
    SET(${commitDate} ${temp} PARENT_SCOPE)
endfunction()

function (getGitRevCount working_dir revCount)
    execute_process (COMMAND git rev-list HEAD --count WORKING_DIRECTORY ${working_dir} OUTPUT_VARIABLE temp)
    string(REPLACE \n "" temp ${temp})
    SET(${revCount} ${temp} PARENT_SCOPE)
endfunction()

