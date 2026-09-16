#include <catch2/catch_test_macros.hpp>
#include <LLUtils/PlatformUtility.h>
#include <FreeTypeWrapper/FreeTypeConnector.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <span>

TEST_CASE("Font loading keeps code-page handling at the native file boundary", "[string][font]")
{
    const auto font = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                      "Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf";
    REQUIRE(std::filesystem::exists(font));
    const auto measure = [](const std::filesystem::path& path)
    {
        FreeType::FreeTypeConnector connector;
        FreeType::TextMesureParams params{};
        params.createParams.fontPath = path.native();
        params.createParams.text     = LLUTILS_TEXT("Unicode \u00e9 \u05d0");
        params.createParams.fontSize = 14;
        params.createParams.DPIx     = 96;
        params.createParams.DPIy     = 96;
        FreeType::TextMetrics metrics;
        connector.MeasureText(params, metrics);
        CHECK_FALSE(metrics.lineMetrics.empty());
    };
    measure(font);
    struct TemporaryFont
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     ("oiv-font-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryFont() { REQUIRE(std::filesystem::create_directory(path)); }
        ~TemporaryFont()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } temporary;
    const auto unicode = temporary.path / std::filesystem::path(u8"font-\u00e9-\u05d0-\U0001f4f7.ttf");
    std::filesystem::copy_file(font, unicode);
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
    const UINT codePage = AreFileApisANSI() ? GetACP() : GetOEMCP();
    if (codePage == CP_UTF8)
        measure(unicode);
    else
    {
        BOOL substituted  = FALSE;
        const auto native = unicode.native();
        const int size    = WideCharToMultiByte(codePage, WC_NO_BEST_FIT_CHARS, native.data(),
                                                static_cast<int>(native.size()), nullptr, 0, nullptr, &substituted);
        if (size == 0 || substituted)
            CHECK_THROWS(measure(unicode));
        else
            measure(unicode);
    }
#else
    measure(unicode);
    // Linux file names are bytes, even when those bytes are not Unicode text.
    const auto raw = temporary.path / std::string("font-\xff.ttf");
    std::filesystem::copy_file(font, raw);
    measure(raw);
#endif
}

TEST_CASE("Cached text measurements preserve rasterized pixels", "[font][bitmap]")
{
    FreeType::FreeTypeConnector connector;
    FreeType::TextCreateParams params{};
    params.fontPath     = (std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                           "Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf")
                              .native();
    params.text         = LLUTILS_TEXT("<textcolor=#ff0000>O<textcolor=#00ff00>I<textcolor=#0000ff>V\nText");
    params.fontSize     = 24;
    params.DPIx         = 96;
    params.DPIy         = 96;
    params.outlineWidth = 3;
    params.flags        = FreeType::TextCreateFlags::UseMetaText;
    FreeType::TextMetrics metrics;
    connector.MeasureText({params}, metrics);
    FreeType::FreeTypeConnector::Bitmap cached;
    FreeType::FreeTypeConnector::Bitmap measured;
    connector.CreateBitmap(params, cached, &metrics);
    connector.CreateBitmap(params, measured, nullptr);

    REQUIRE(cached.buffer.size() > 0);
    CHECK(cached.width == measured.width);
    CHECK(cached.height == measured.height);
    CHECK(cached.rowPitch == measured.rowPitch);
    CHECK(std::ranges::equal(std::span{cached.buffer.data(), cached.buffer.size()},
                             std::span{measured.buffer.data(), measured.buffer.size()}));
}

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
// Run this hidden case in a separate executable with a UTF-8 activeCodePage manifest.
// Changing LC_CTYPE does not change the Windows file API code page.
TEST_CASE("UTF-8 process manifest selects the UTF-8 file code page", "[.][font-utf8-acp]")
{
    REQUIRE(GetACP() == CP_UTF8);
}

TEST_CASE("Repeated text measurements release temporary glyphs", "[font][memory]")
{
    FreeType::FreeTypeConnector connector;
    FreeType::TextMesureParams params{};
    params.createParams.fontPath = (std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                                    "Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf")
                                       .native();
    params.createParams.text     = LLUTILS_TEXT("<textcolor=#4a80e2>Welcome to OIV\nDrag here an image to start\n"
                                                "Press F1 to show key bindings");
    params.createParams.DPIx     = 96;
    params.createParams.DPIy     = 96;
    params.createParams.flags    = FreeType::TextCreateFlags::UseMetaText | FreeType::TextCreateFlags::Bidirectional;
    FreeType::TextMetrics metrics;
    const auto measureSizes = [&]()
    {
        for (uint16_t fontSize = 14; fontSize <= 44; ++fontSize)
        {
            params.createParams.fontSize = fontSize;
            connector.MeasureText(params, metrics);
        }
    };

    // FreeType uses the process heap directly. Report only failures here: successful Catch2 assertions
    // can allocate reporter records and contaminate the next snapshot.
    const auto allocatedBytes = []()
    {
        const HANDLE heap = GetProcessHeap();
        if (!HeapLock(heap))
            FAIL("Unable to lock the process heap.");
        PROCESS_HEAP_ENTRY entry{};
        size_t bytes = 0;
        while (HeapWalk(heap, &entry))
        {
            if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0)
                bytes += entry.cbData;
        }
        const DWORD error = GetLastError();
        if (!HeapUnlock(heap) || error != ERROR_NO_MORE_ITEMS)
            FAIL("Unable to walk or unlock the process heap.");
        return bytes;
    };

    for (const uint32_t outlineWidth : {0u, 3u})
    {
        params.createParams.outlineWidth = outlineWidth;
        // Warm FreeType's caches before checking that resizing leaves no extra live allocations.
        measureSizes();
        const size_t before = allocatedBytes();
        measureSizes();
        const size_t after = allocatedBytes();
        CAPTURE(outlineWidth);
        CHECK(after == before);
    }
}
#endif
