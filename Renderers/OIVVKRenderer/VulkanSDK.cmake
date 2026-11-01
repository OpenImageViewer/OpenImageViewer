# A pinned binary SDK gives local builds and CI the same headers and shader compiler
# without building shaderc from source. The tradeoff is a larger cached download and
# a 7-Zip dependency on Windows; the installer is never run. Explicit paths or
# VULKAN_SDK bypass acquisition. Linux resolves its target Vulkan loader separately.
# Package filenames/checksums: https://vulkan.lunarg.com/sdk/files.json
if(NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    find_path(OIV_VULKAN_INCLUDE_DIR vulkan/vulkan.h PATHS "$ENV{VULKAN_SDK}"
        PATH_SUFFIXES Include include NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_program(OIV_GLSLC glslc PATHS "$ENV{VULKAN_SDK}"
        PATH_SUFFIXES Bin bin NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    if(NOT OIV_VULKAN_INCLUDE_DIR OR NOT OIV_GLSLC)
        message(FATAL_ERROR "VULKAN_SDK must contain Vulkan headers and a host glslc. Correct it or unset it to download the pinned SDK.")
    endif()
endif()
if(OIV_VULKAN_INCLUDE_DIR AND NOT EXISTS "${OIV_VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "OIV_VULKAN_INCLUDE_DIR does not contain vulkan/vulkan.h: ${OIV_VULKAN_INCLUDE_DIR}")
endif()
if(OIV_GLSLC AND NOT EXISTS "${OIV_GLSLC}")
    message(FATAL_ERROR "OIV_GLSLC must point to a host glslc executable: ${OIV_GLSLC}")
endif()

if(NOT OIV_VULKAN_INCLUDE_DIR OR NOT OIV_GLSLC)
    set(oivSdkVersion 1.4.357.0)
    # Select runnable tools by HOST, including Linux-to-Windows cross-builds.
    if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$" OR
       NOT (CMAKE_HOST_WIN32 OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"))
        message(FATAL_ERROR "Automatic Vulkan SDK downloads support Windows x64 and Linux x86_64 hosts. Set OIV_VULKAN_INCLUDE_DIR and OIV_GLSLC for this host.")
    endif()
    if(CMAKE_HOST_WIN32)
        set(oivSdkPlatform windows)
        set(oivSdkFile "vulkansdk-windows-X64-${oivSdkVersion}.exe")
        set(oivSdkHash 81f474711e9042f4cd22b31b2f7a8870db2e428b21586fb43dd80150be97310d)
        set(oivSdkInclude Include)
        set(oivSdkCompiler Bin/glslc.exe)
    else()
        set(oivSdkPlatform linux)
        set(oivSdkFile "vulkansdk-linux-x86_64-${oivSdkVersion}.tar.xz")
        set(oivSdkHash 0f09bf6a0625e346bf004be70b92907e934a4c76606b323441b2baf3a5a0e66d)
        set(oivSdkInclude "${oivSdkVersion}/x86_64/include")
        set(oivSdkCompiler "${oivSdkVersion}/x86_64/bin/glslc")
    endif()
    set(oivSdkCache "${CMAKE_BINARY_DIR}/_deps/vulkan-sdk/${oivSdkVersion}/${oivSdkPlatform}-x64")
    set(oivSdkRoot "${oivSdkCache}/sdk")
    if(NOT EXISTS "${oivSdkRoot}/.complete" OR
       NOT EXISTS "${oivSdkRoot}/${oivSdkInclude}/vulkan/vulkan.h" OR
       NOT EXISTS "${oivSdkRoot}/${oivSdkCompiler}")
        # An interrupted repair must not leave a previous completion marker valid.
        file(REMOVE "${oivSdkRoot}/.complete")
        if(CMAKE_HOST_WIN32)
            find_program(oivSdk7Zip NAMES 7z 7zz PATHS "$ENV{ProgramFiles}/7-Zip"
                NO_CMAKE_FIND_ROOT_PATH)
            if(NOT oivSdk7Zip)
                message(FATAL_ERROR "Unpacking the Windows Vulkan SDK requires 7-Zip. Install it or supply OIV_VULKAN_INCLUDE_DIR and OIV_GLSLC.")
            endif()
        endif()
        file(MAKE_DIRECTORY "${oivSdkCache}")
        message(STATUS "Downloading Vulkan SDK ${oivSdkVersion} for ${oivSdkPlatform} x64")
        file(DOWNLOAD "https://sdk.lunarg.com/sdk/download/${oivSdkVersion}/${oivSdkPlatform}/${oivSdkFile}"
            "${oivSdkCache}/${oivSdkFile}" EXPECTED_HASH "SHA256=${oivSdkHash}"
            TLS_VERIFY ON TIMEOUT 600 INACTIVITY_TIMEOUT 30 STATUS oivSdkDownload SHOW_PROGRESS)
        list(GET oivSdkDownload 0 oivSdkDownloadCode)
        if(NOT oivSdkDownloadCode EQUAL 0)
            message(FATAL_ERROR "Vulkan SDK download failed: ${oivSdkDownload}. Supply OIV_VULKAN_INCLUDE_DIR and OIV_GLSLC for offline builds.")
        endif()
        if(CMAKE_HOST_WIN32)
            # Qt's installer embeds several archives; ordinary extraction sees only one.
            execute_process(COMMAND "${oivSdk7Zip}" x "-t#" -y "-o${oivSdkCache}/archives"
                "${oivSdkCache}/${oivSdkFile}" "*.7z"
                COMMAND_ERROR_IS_FATAL ANY OUTPUT_QUIET)
            file(GLOB oivSdkArchives "${oivSdkCache}/archives/*.7z")
            foreach(oivSdkArchive IN LISTS oivSdkArchives)
                execute_process(COMMAND "${oivSdk7Zip}" x -y "-o${oivSdkRoot}" "${oivSdkArchive}"
                    "-i!Include/*" "-i!Bin/glslc.exe" COMMAND_ERROR_IS_FATAL ANY OUTPUT_QUIET)
            endforeach()
            file(REMOVE ${oivSdkArchives})
        else()
            file(ARCHIVE_EXTRACT INPUT "${oivSdkCache}/${oivSdkFile}" DESTINATION "${oivSdkRoot}"
                PATTERNS "${oivSdkInclude}/*" "${oivSdkCompiler}")
        endif()
        if(NOT EXISTS "${oivSdkRoot}/${oivSdkInclude}/vulkan/vulkan.h" OR
           NOT EXISTS "${oivSdkRoot}/${oivSdkCompiler}")
            message(FATAL_ERROR "Vulkan SDK extraction is incomplete: ${oivSdkRoot}")
        endif()
        file(WRITE "${oivSdkRoot}/.complete" "${oivSdkHash}\r\n")
    endif()
    # Keep managed paths out of the CMake cache so changing the pin takes effect.
    if(NOT OIV_VULKAN_INCLUDE_DIR)
        set(OIV_VULKAN_INCLUDE_DIR "${oivSdkRoot}/${oivSdkInclude}")
    endif()
    if(NOT OIV_GLSLC)
        set(OIV_GLSLC "${oivSdkRoot}/${oivSdkCompiler}")
    endif()
endif()

execute_process(COMMAND "${OIV_GLSLC}" --version RESULT_VARIABLE oivGlslcResult
    OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
if(NOT "${oivGlslcResult}" STREQUAL "0")
    message(FATAL_ERROR "OIV_GLSLC must run on the build host: ${OIV_GLSLC}. Check its runtime dependencies or supply another host glslc.")
endif()
message(STATUS "Vulkan headers: ${OIV_VULKAN_INCLUDE_DIR}")
message(STATUS "Vulkan host glslc: ${OIV_GLSLC}")
