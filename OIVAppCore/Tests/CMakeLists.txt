file(GLOB testSources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Test*.cpp")

add_executable(tests_oivappcore
    ${testSources}
    ImageMagickBrowsingCorpus.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/ImageCodec/Tests/ImageMagickTestCorpus.cpp"
)
target_include_directories(tests_oivappcore PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source/Src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/ImageCodec/Tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/json/single_include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../External/ExoticNumbers/include"
)
target_compile_definitions(tests_oivappcore PRIVATE
    "OIV_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}/Tests\""
    "OIV_IMAGE_CORPUS_DIR=\"${CMAKE_SOURCE_DIR}/External/ImageCodec/Tests\""
)
target_link_libraries(tests_oivappcore PRIVATE
    Catch2::Catch2WithMain
    CLI11::CLI11
    oivappcore
    oivshared
    RendererCore
    ImageCodec
    ImageUtil
    tiff
    LWSLib
)

if(TARGET CodecPNG AND TARGET CodecJPG)
    target_compile_definitions(tests_oivappcore PRIVATE OIV_TEST_TRANSFER_ENCODERS=1)
endif()
if(TARGET CodecFreeImage)
    target_compile_definitions(tests_oivappcore PRIVATE OIV_TEST_DNG_FALLBACK=1)
endif()
if(TARGET libjpeg-turbo::libjpeg-turbo)
    target_link_libraries(tests_oivappcore PRIVATE libjpeg-turbo::libjpeg-turbo)
endif()

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_source_files_properties(TestSequencer.cpp TestTransferFunction.cpp PROPERTIES COMPILE_OPTIONS "/fp:precise")
else()
    set_source_files_properties(TestTransferFunction.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
endif()
add_test(NAME oivappcore.unit COMMAND tests_oivappcore --reporter compact)

add_executable(tests_storage_process StorageProcess.cpp)
target_link_libraries(tests_storage_process PRIVATE oivappcore)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_link_options(tests_storage_process PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
elseif(MINGW)
    target_link_options(tests_storage_process PRIVATE "-mconsole" "-municode")
endif()

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_link_options(tests_oivappcore PRIVATE "/SUBSYSTEM:CONSOLE" "/ENTRY:wmainCRTStartup")
elseif(MINGW)
    set_target_properties(tests_oivappcore PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_options(tests_oivappcore PRIVATE "-mconsole" "-municode")
endif()
