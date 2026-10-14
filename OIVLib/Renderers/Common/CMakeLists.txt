add_library(RendererCore STATIC Source/Renderer.cpp Source/RendererException.cpp)
target_include_directories(RendererCore PUBLIC Include)
target_link_libraries(RendererCore PUBLIC ImageCodec LLUtils::LLUtils)
