add_library(RendererCore STATIC Source/Renderer.cpp)
target_include_directories(RendererCore PUBLIC Include ../../OIV/Include)
target_link_libraries(RendererCore PUBLIC ImageCodec LLUtils::LLUtils)
