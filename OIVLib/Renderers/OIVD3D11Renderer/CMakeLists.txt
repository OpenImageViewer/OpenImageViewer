#OIVD3DRenderer
cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName OIVD3D11Renderer)
add_library (${TargetName} ${sourceFiles} )
# Compile and link a tiny program to report missing SDK libraries during configure,
# before building the application. The program is not run. Use the target toolchain's
# linker because standalone clang-cl can discover the Windows SDK even when
# find_library cannot find it through LIB or CMake's implicit search paths.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
set(oivD3D11Libraries dxgi d3dcompiler d3d11 dxguid)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES ${oivD3D11Libraries})
# This directory's SDK probe must link even if the toolchain defaults to static probes.
set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
check_cxx_source_compiles("int main() { return 0; }" OIV_D3D11_LIBRARIES_AVAILABLE)
cmake_pop_check_state()
if(NOT OIV_D3D11_LIBRARIES_AVAILABLE)
    message(FATAL_ERROR
        "The target toolchain could not link the D3D11 SDK libraries (dxgi, d3dcompiler, d3d11, dxguid). "
        "Make sure the Windows SDK kit is installed correctly and available to the selected toolchain, "
        "or set OIV_BUILD_RENDERER_D3D11=OFF.")
endif()
target_link_libraries(${TargetName} PRIVATE ${oivD3D11Libraries} LLUtils::LLUtils)
