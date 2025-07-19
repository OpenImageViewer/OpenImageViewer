#OIVD3DRenderer
cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE sourceFiles
    "./Source/*.h"
    "./Source/*.cpp"
)

set(TargetName OIVD3D11Renderer)
add_library (${TargetName} ${sourceFiles} )
target_link_libraries(${TargetName} 
dbghelp.lib
dxgi.lib
d3dcompiler.lib
d3d11.lib
dxguid.lib)

