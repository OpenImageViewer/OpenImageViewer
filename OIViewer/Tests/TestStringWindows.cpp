#include <catch2/catch_test_macros.hpp>
#include <LLUtils/StringUtility.h>

#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
    #include <LLUtils/PlatformUtility.h>
    #include <LWS/Clipboard.hpp>
    #include <LWS/Win32/Platform.hpp>
    #include <LWS/Window.hpp>
    #include <array>
    #include <thread>
    #include <chrono>

TEST_CASE("Windows error text uses Unicode independently of the C locale", "[string][win32]")
{
    SetLastError(ERROR_FILE_NOT_FOUND);
    const auto wide = LLUtils::PlatformUtility::GetLastErrorAsString<wchar_t>();
    REQUIRE_FALSE(wide.empty());
    SetLastError(ERROR_FILE_NOT_FOUND);
    CHECK(LLUtils::PlatformUtility::GetLastErrorAsString<char>() ==
          LLUtils::StringUtility::ConvertString<std::string>(wide));
    SetLastError(ERROR_FILE_NOT_FOUND);
    CHECK(LLUtils::PlatformUtility::GetLastErrorAsString<char8_t>() ==
          LLUtils::StringUtility::ConvertString<std::u8string>(wide));
    SetLastError(ERROR_SUCCESS);
    CHECK(LLUtils::PlatformUtility::GetLastErrorAsString<char>().empty());
    SetLastError(0xffffffff);
    CHECK(LLUtils::PlatformUtility::GetLastErrorAsString<char>().empty());
}

// Explicit run: tests.exe "[clipboard]". This test changes the desktop clipboard;
// use an external harness to snapshot and restore its contents. Hidden from the default suite.
TEST_CASE("Clipboard publishes Unicode and Windows synthesizes legacy text", "[.][clipboard][win32]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    LWS::Window window(platform);
    REQUIRE(window.Create({.visible = false}) == LWS::Result::Success);
    LWS::Clipboard clipboard(platform);
    clipboard.RegisterFormat(CF_UNICODETEXT);
    const auto expected = L"Clipboard \u00e9 \u05d0 \U0001f4f7";
    REQUIRE(clipboard.SetClipboardText(window, "Clipboard \xc3\xa9 \xd7\x90 \xf0\x9f\x93\xb7") ==
            LWS::ClipboardResult::Success);
    REQUIRE(IsClipboardFormatAvailable(CF_TEXT));
    const auto [format, buffer] = clipboard.GetClipboardData();
    REQUIRE(format == CF_UNICODETEXT);
    REQUIRE(buffer.size() >= sizeof(wchar_t));
    CHECK(std::wstring_view(reinterpret_cast<const wchar_t*>(buffer.data())) == expected);

    CHECK(clipboard.SetClipboardText(window, "\xff") == LWS::ClipboardResult::UnknownError);
    CHECK(clipboard.SetClipboardText(window, static_cast<const char*>(nullptr)) == LWS::ClipboardResult::UnknownError);
    const auto [unchangedFormat, unchanged] = clipboard.GetClipboardData();
    CHECK(unchangedFormat == CF_UNICODETEXT);
    CHECK(std::wstring_view(reinterpret_cast<const wchar_t*>(unchanged.data())) == expected);

    REQUIRE(clipboard.SetClipboardText(window, L"legacy ASCII") == LWS::ClipboardResult::Success);
    LWS::Clipboard legacy(platform);
    legacy.RegisterFormat(CF_TEXT);
    const auto [legacyFormat, legacyBytes] = legacy.GetClipboardData();
    REQUIRE(legacyFormat == CF_TEXT);
    CHECK(std::string_view(reinterpret_cast<const char*>(legacyBytes.data())) == "legacy ASCII");

    // Exercise format preference through the service without interpreting image pixels.
    BITMAPV5HEADER header{};
    header.bV5Size      = sizeof(header);
    header.bV5Width     = 1;
    header.bV5Height    = 1;
    header.bV5Planes    = 1;
    header.bV5BitCount  = 32;
    header.bV5SizeImage = 4;
    std::array<std::byte, sizeof(header) + 4> image{};
    std::memcpy(image.data(), &header, sizeof(header));
    const auto text = std::wstring_view(L"text");
    const std::array data{
        LWS::ClipboardDataView{.format = CF_UNICODETEXT,
                               .data   = {reinterpret_cast<const std::byte*>(text.data()),
                                          (text.size() + 1) * sizeof(wchar_t)}},
        LWS::ClipboardDataView{.format = CF_DIB, .data = image},
        LWS::ClipboardDataView{.format = CF_DIBV5, .data = image},
    };
    // Clipboard observers can briefly own the clipboard after a publication.
    auto publication = LWS::ClipboardResult::AccessDenied;
    for (unsigned attempt = 0; attempt < 50 && publication == LWS::ClipboardResult::AccessDenied; ++attempt)
    {
        publication = clipboard.SetClipboardData(window, data);
        if (publication == LWS::ClipboardResult::AccessDenied)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    REQUIRE(publication == LWS::ClipboardResult::Success);
    LWS::Clipboard imagesFirst(platform);
    imagesFirst.RegisterFormat(CF_DIBV5);
    imagesFirst.RegisterFormat(CF_DIB);
    imagesFirst.RegisterFormat(CF_UNICODETEXT);
    CHECK(std::get<LWS::ClipboardFormatType>(imagesFirst.GetClipboardData()) == CF_DIBV5);
}
#endif
