add_executable(tests_vk_renderer TestOIVVKRenderer.cpp)
target_compile_definitions(tests_vk_renderer PRIVATE OIV_TEST_VK_RENDERER)
target_link_libraries(tests_vk_renderer PRIVATE Catch2::Catch2WithMain OIVVKRenderer)
add_test(NAME renderer.vulkan.unit COMMAND tests_vk_renderer --reporter compact)

# Keep injected failure paths isolated from the normal renderer test process and real loader.
add_executable(tests_vk_failures
    FailureTests.cpp
    ../Source/VKContext.cpp
    ../Source/VKPipeline.cpp
    ../Source/VKRenderer.cpp
    ../Source/VKTexture.cpp
)
add_dependencies(tests_vk_failures VKShaders)
target_compile_definitions(tests_vk_failures PRIVATE
    OIV_VK_TEST_DOUBLES
    $<TARGET_PROPERTY:OIVVKRenderer,COMPILE_DEFINITIONS>
)
if(WIN32)
    target_link_libraries(tests_vk_failures PRIVATE user32)
endif()
target_include_directories(tests_vk_failures PRIVATE
    ../Source
    ../../../OIVAppCore/Source/Include
)
target_link_libraries(tests_vk_failures PRIVATE
    Catch2::Catch2WithMain
    RendererCore
    ImageCodec
    ImageUtil
    LLUtils::LLUtils
    Vulkan::Headers
)
add_test(NAME renderer.vulkan.failures COMMAND tests_vk_failures --reporter compact)

foreach(testTarget IN ITEMS tests_vk_renderer tests_vk_failures)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_link_options(${testTarget} PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
    elseif(MINGW)
        set_target_properties(${testTarget} PROPERTIES WIN32_EXECUTABLE FALSE)
        target_link_options(${testTarget} PRIVATE "-mconsole" "-municode")
    endif()
endforeach()
