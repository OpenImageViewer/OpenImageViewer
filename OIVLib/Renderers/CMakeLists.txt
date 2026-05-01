if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(OIVD3D11Renderer)
endif()
add_subdirectory(OIVGLRenderer)