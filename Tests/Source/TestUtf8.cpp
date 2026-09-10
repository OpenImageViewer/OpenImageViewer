#include <catch2/catch_test_macros.hpp>
#include <LLUtils/StringUtility.h>
#include <array>
#include <clocale>
#include <sstream>

// OIViewer text uses UTF-8 in char/char8_t and UTF-16 at native Windows interfaces.
// Encoding follows this application contract, so locale changes must not reinterpret the same bytes.
// Native Linux filenames can contain non-UTF-8 bytes; preserve paths and transcode only Unicode text.
using LLUtils::StringUtility;
using namespace std::string_view_literals;
using namespace std::string_literals;

namespace
{
    template <class Destination, class Source>
    concept CanConvert = requires(Source source) { StringUtility::ConvertString<Destination>(source); };

    static_assert(CanConvert<std::string, std::wstring_view>);
    static_assert(CanConvert<std::u8string, const char*>);
    static_assert(!CanConvert<std::u16string, std::string>);
    static_assert(!CanConvert<std::string, std::u16string>);
    static_assert(!CanConvert<std::string, int>);
    static_assert(std::is_same_v<decltype(StringUtility::ToAString("")), const char*>);

    struct LocaleRestore
    {
        std::string previous = std::setlocale(LC_CTYPE, nullptr);
        ~LocaleRestore() { std::setlocale(LC_CTYPE, previous.c_str()); }
    };
}  // namespace

TEST_CASE("String conversion accepts all supported representations and input forms", "[string][utf8]")
{
    const std::string bytes  = "GPU \xc3\xa9 \xd7\x90 \xf0\x9f\x93\xb7";
    const std::u8string utf8 = u8"GPU \u00e9 \u05d0 \U0001f4f7";
    const std::wstring wide  = L"GPU \u00e9 \u05d0 \U0001f4f7";
    const auto check         = [&]<class Source>(const Source& source)
    {
        CHECK(StringUtility::ConvertString<std::string>(source) == bytes);
        CHECK(StringUtility::ConvertString<std::u8string>(source) == utf8);
        CHECK(StringUtility::ConvertString<std::wstring>(source) == wide);
    };
    check(bytes);
    check(utf8);
    check(wide);
    check(std::string_view(bytes));
    check(std::u8string_view(utf8));
    check(std::wstring_view(wide));
    check(bytes.c_str());
    check(utf8.c_str());
    check(wide.c_str());
    CHECK(StringUtility::ConvertString<std::string>("same") == "same");
    CHECK(StringUtility::ConvertString<std::u8string>(u8"same") == u8"same");
    CHECK(StringUtility::ConvertString<std::wstring>(L"same") == L"same");
    CHECK(StringUtility::ToAString(wide) == bytes);
    CHECK(StringUtility::ToAString(wide.c_str()) == bytes);
    CHECK(StringUtility::ToWString(bytes) == wide);
    CHECK(StringUtility::ToNativeString(utf8) == LLUTILS_TEXT("GPU \u00e9 \u05d0 \U0001f4f7"));
    CHECK(StringUtility::ToDefaultString(utf8) == StringUtility::ToNativeString(utf8));
    CHECK(StringUtility::ConvertString<std::string>(std::wstring_view{}).empty());
    CHECK(StringUtility::ConvertString<std::u8string>(std::string_view{}).empty());
    CHECK(StringUtility::ConvertString<std::wstring>(std::u8string_view{}).empty());
    CHECK(StringUtility::ConvertString<std::wstring>(std::wstring_view{}).empty());
}

TEST_CASE("Conversions preserve lengths and only transcode at byte wide boundaries", "[string][utf8]")
{
    const std::string bytes("a\0b", 3);
    const std::wstring wide(L"a\0b", 3);
    const std::u8string utf8(u8"a\0b", 3);
    CHECK(StringUtility::ConvertString<std::wstring>(bytes) == wide);
    CHECK(StringUtility::ConvertString<std::wstring>(std::string_view(bytes)) == wide);
    CHECK(StringUtility::ConvertString<std::string>(wide) == bytes);
    CHECK(StringUtility::ConvertString<std::u8string>(wide) == utf8);
    CHECK(StringUtility::ConvertString<std::wstring>(utf8) == wide);
    CHECK(StringUtility::ConvertString<std::string>(utf8) == bytes);
    CHECK(StringUtility::ConvertString<std::u8string>(bytes) == utf8);
    CHECK(StringUtility::ConvertString<std::wstring>(bytes.c_str()) == L"a");
    CHECK(StringUtility::ConvertString<std::string>(wide.c_str()) == "a");
    CHECK(StringUtility::ConvertString<std::wstring>(utf8.c_str()) == L"a");

    const std::string invalid("\xff\x80", 2);
    CHECK(StringUtility::ConvertString<std::string>(invalid) == invalid);
    const auto typed = StringUtility::ConvertString<std::u8string>(invalid);
    CHECK(StringUtility::ConvertString<std::string>(typed) == invalid);
    const std::wstring invalidWide(1, static_cast<wchar_t>(0xd800));
    CHECK(StringUtility::ConvertString<std::wstring>(invalidWide) == invalidWide);
    CHECK_THROWS_AS(StringUtility::ConvertString<std::wstring>(typed), std::invalid_argument);
    CHECK_THROWS_AS(StringUtility::ConvertString<std::wstring>(static_cast<const char*>(nullptr)),
                    std::invalid_argument);
    CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(static_cast<const char8_t*>(nullptr)),
                    std::invalid_argument);
    CHECK_THROWS_AS(StringUtility::ConvertString<std::u8string>(static_cast<const wchar_t*>(nullptr)),
                    std::invalid_argument);

    std::string owned(256, 'x');
    const auto* storage = owned.data();
    const auto moved    = StringUtility::ConvertString<std::string>(std::move(owned));
    CHECK(moved.data() == storage);
}

TEST_CASE("Unicode scalar boundaries have the expected UTF-8 encoding", "[string][utf8]")
{
    const auto wide  = L"\x7f\x80\x7ff\x800\xd7ff\xe000\xffff\U00010000\U0010ffff"sv;
    const auto bytes = "\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf"
                       "\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"sv;
    CHECK(StringUtility::ConvertString<std::string>(wide) == bytes);
    CHECK(StringUtility::ConvertString<std::wstring>(bytes) == wide);
    // A BOM is ordinary data, and canonically equivalent strings are not normalized.
    CHECK(StringUtility::ConvertString<std::string>(L"\ufeffe\u0301"sv) == "\xef\xbb\xbf"
                                                                           "e\xcc\x81");
}

TEST_CASE("Transcoding rejects malformed Unicode", "[string][utf8]")
{
    const std::array invalid{
        "\x80"sv,         "\xc0\xaf"sv,         "\xc1\xbf"sv,         "\xc2"sv,         "\xc2\x20"sv,
        "\xe0\x80\x80"sv, "\xe1\x80"sv,         "\xed\xa0\x80"sv,     "\xed\xbf\xbf"sv, "\xf0\x80\x80\x80"sv,
        "\xf0\x90\x80"sv, "\xf4\x90\x80\x80"sv, "\xf5\x80\x80\x80"sv, "\xfe"sv,         "\xff"sv,
        "a\0\xff"sv,      "\xe2\0\xa1"sv};
    for (const auto bytes : invalid)
    {
        CAPTURE(bytes.size());
        CHECK_THROWS_AS(StringUtility::ConvertString<std::wstring>(bytes), std::invalid_argument);
        CHECK_THROWS_AS(StringUtility::ConvertString<std::wstring>(StringUtility::ConvertString<std::u8string>(bytes)),
                        std::invalid_argument);
    }
    for (const wchar_t value : {static_cast<wchar_t>(0xd800), static_cast<wchar_t>(0xdbff),
                                static_cast<wchar_t>(0xdc00), static_cast<wchar_t>(0xdfff)})
    {
        CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(std::wstring(1, value)), std::invalid_argument);
        CHECK_THROWS_AS(StringUtility::ConvertString<std::u8string>(std::wstring(1, value)), std::invalid_argument);
    }
    CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(L"\xd800"
                                                              L"A"sv),
                    std::invalid_argument);
    CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(L"\xdc00\xd800"sv), std::invalid_argument);
    if constexpr (sizeof(wchar_t) == 4)
    {
        CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(std::wstring(1, static_cast<wchar_t>(0x110000))),
                        std::invalid_argument);
        CHECK_THROWS_AS(StringUtility::ConvertString<std::string>(std::wstring(1, static_cast<wchar_t>(-1))),
                        std::invalid_argument);
    }
}

TEST_CASE("Renderer conversion and ASCII casing are independent of the C locale", "[renderer][string][utf8]")
{
    LocaleRestore restore;
    for (const auto* locale : {"C", ".1252", ".UTF-8", "C.UTF-8", "en_US.UTF-8"})
    {
        if (std::setlocale(LC_CTYPE, locale) == nullptr)
            continue;
        CAPTURE(locale);
        const LLUtils::native_string_type native = LLUTILS_TEXT("GPU \u00e9 \u05d0 \U0001f4f7");
        const std::string encoded                = "GPU \xc3\xa9 \xd7\x90 \xf0\x9f\x93\xb7";
        CHECK(StringUtility::ConvertString<std::string>(native) == encoded);
        CHECK(StringUtility::ConvertString<LLUtils::native_string_type>(encoded) == native);
        CHECK(StringUtility::ConvertString<std::wstring>("\xc3\xa9") == L"\u00e9");
        CHECK_THROWS_AS(StringUtility::ConvertString<std::wstring>("\xe9"), std::invalid_argument);
        CHECK(StringUtility::ToLower("A\xc3\x89Z"s) == "a\xc3\x89z");
        CHECK(StringUtility::ToUpper("a\xc3\xa9z"s) == "A\xc3\xa9Z");
        CHECK(StringUtility::ToLower(u8"AZ\u00c9\u05d0\U0001f4f7"s) == u8"az\u00c9\u05d0\U0001f4f7");
        CHECK(StringUtility::ToUpper(L"az\u00e9\u05d0\U0001f4f7"s) == L"AZ\u00e9\u05d0\U0001f4f7");
    }
}

// StrCpy trusts valid, NUL-free input and its supplied length; it checks only the cutoff boundary.
// Truncation preserves code points, but can split a grapheme cluster. It never scans for a terminator or pads the
// destination.
TEST_CASE("Bounded copies terminate without splitting code points or padding", "[string][utf8]")
{
    const auto check = []<class Char>(std::basic_string_view<Char> source)
    {
        for (std::size_t capacity = 1; capacity <= source.size() + 2; ++capacity)
        {
            std::basic_string<Char> buffer(source.size() + 3, static_cast<Char>('!'));
            const auto result = StringUtility::StrCpy(buffer.data(), source, capacity);
            CAPTURE(capacity, result.written);
            CHECK(result.written < capacity);
            CHECK(buffer[result.written] == Char{});
            CHECK(buffer[result.written + 1] == static_cast<Char>('!'));
            CHECK(std::basic_string_view<Char>(buffer.data(), result.written) == source.substr(0, result.written));
            CHECK(result.truncated == (result.written < source.size()));
            // Strict transcoding proves every emitted prefix ends at a scalar boundary.
            if constexpr (std::is_same_v<Char, wchar_t>)
                CHECK_NOTHROW(StringUtility::ConvertString<std::string>(
                    std::basic_string_view<Char>(buffer.data(), result.written)));
            else
                CHECK_NOTHROW(StringUtility::ConvertString<std::wstring>(
                    std::basic_string_view<Char>(buffer.data(), result.written)));
        }
    };
    check("a\xc3\xa9\xf0\x9f\x93\xb7z"sv);
    check(u8"a\u00e9\U0001f4f7z"sv);
    check(L"a\u00e9\U0001f4f7z"sv);
    char bytes[4]{};
    const auto result = StringUtility::StrCpy(bytes, "a\xc3\xa9z"sv);
    CHECK(result.written == 3);
    CHECK(result.truncated);
    CHECK(std::string_view(bytes) == "a\xc3\xa9");
    const auto zero = StringUtility::StrCpy(static_cast<char*>(nullptr), ""sv, 0);
    CHECK(zero.written == 0);
    CHECK(zero.truncated);
    CHECK_THROWS_AS(StringUtility::StrCpy(static_cast<char*>(nullptr), ""sv, 1), std::invalid_argument);
}

TEST_CASE("Bounded copies preserve overlap and trust view lengths", "[string][utf8]")
{
    char bytes[12] = "abcdef";
    CHECK(StringUtility::StrCpy(bytes + 1, std::string_view(bytes), 7).written == 6);
    CHECK(std::string_view(bytes + 1) == "abcdef");
    CHECK(StringUtility::StrCpy(bytes, std::string_view(bytes + 2), 12).written == 5);
    CHECK(std::string_view(bytes) == "bcdef");
    const auto empty = StringUtility::StrCpy(bytes, ""sv);
    CHECK(empty.written == 0);
    CHECK_FALSE(empty.truncated);
    CHECK(bytes[0] == '\0');
    // This view has no terminator: StrCpy must use its length without reading past it.
    const char raw[]  = {'x', 'y', 'z'};
    const auto copied = StringUtility::StrCpy(bytes, std::string_view(raw, 3));
    CHECK(copied.written == 3);
    CHECK_FALSE(copied.truncated);
    CHECK(std::string_view(bytes) == "xyz");
}

TEST_CASE("Direct splitting preserves stream token semantics", "[string]")
{
    const auto reference = [](const auto& source, auto delimiter)
    {
        using String = std::remove_cvref_t<decltype(source)>;
        std::basic_stringstream<typename String::value_type> stream(source);
        std::vector<String> result;
        String item;
        while (std::getline(stream, item, delimiter))
        {
            if (!item.empty())
            {
                while (!item.empty() && item.back() == typename String::value_type{})
                    item.pop_back();
                result.push_back(item);
            }
        }
        return result;
    };
    for (const auto& source : {""s, ","s, ",a,,b,"s, "a\0\0,b\0c,\0\0,"s,
                               std::string(64, 'a') + ',' + std::string(80, 'b') + ',' + std::string(72, 'c')})
    {
        for (const auto delimiter : {',', '\0'})
        {
            CHECK(StringUtility::split(source, delimiter) == reference(source, delimiter));
            const auto wide = StringUtility::ConvertString<std::wstring>(source);
            CHECK(StringUtility::split(wide, static_cast<wchar_t>(delimiter)) ==
                  reference(wide, static_cast<wchar_t>(delimiter)));
            const auto typed    = StringUtility::ConvertString<std::u8string>(source);
            const auto expected = reference(source, delimiter);
            const auto actual   = StringUtility::split(typed, static_cast<char8_t>(delimiter));
            REQUIRE(actual.size() == expected.size());
            for (std::size_t index = 0; index < actual.size(); ++index)
                CHECK(StringUtility::ConvertString<std::string>(actual[index]) == expected[index]);
        }
    }
    for (auto source : {""s, "   "s, "  value  "s})
    {
        StringUtility::trim(source, " ");
        CHECK((source.empty() || source == "value"));
    }
}
