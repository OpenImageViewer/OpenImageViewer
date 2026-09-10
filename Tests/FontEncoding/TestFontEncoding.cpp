#include <catch2/catch_test_macros.hpp>
#include <LLUtils/PlatformUtility.h>
#include <FreeTypeWrapper/FreeTypeConnector.h>
#include <filesystem>
#include <chrono>

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

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
// Run this hidden case in a separate executable with a UTF-8 activeCodePage manifest.
// Changing LC_CTYPE does not change the Windows file API code page.
TEST_CASE("UTF-8 process manifest selects the UTF-8 file code page", "[.][font-utf8-acp]")
{
    REQUIRE(GetACP() == CP_UTF8);
}
#endif
