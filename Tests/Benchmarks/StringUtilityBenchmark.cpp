// Standalone optimized benchmark; run from the repository root after configuring the build directory.
// Windows requires a Visual Studio development shell with clang-cl on PATH.
// clang-format off
// clang-cl /nologo /std:c++23preview /O2 /EHsc /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /IExternal/LLUtils/Include Tests/Benchmarks/StringUtilityBenchmark.cpp /Febuild/windows-clang/string-benchmark.exe /Fobuild/windows-clang/string-benchmark.obj
// ./build/windows-clang/string-benchmark.exe
// clang++ -std=c++23 -O3 -IExternal/LLUtils/Include Tests/Benchmarks/StringUtilityBenchmark.cpp -o build/linux-clang/string-benchmark
// build/linux-clang/string-benchmark
// clang-format on
// Seven batches rotate implementation order, consume results, and report median ns/call plus allocations.
// Both converters receive identical valid, NUL-free text under a UTF-8 locale; malformed input is not timed.
// Copy guarantees differ: strncpy/wcsncpy scan and pad, and may omit a terminator; StrCpy trusts a valid
// NUL-free view and appends a terminator without padding. Same-type moves reuse existing storage.
// These helper timings exclude filesystem-extension costs and do not establish viewer frame-rate gains
// or a comparison against equivalent Win32 Unicode APIs. Measurements are observations, not test thresholds.
#include <LLUtils/StringUtility.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <sstream>
#include <memory>
#include <new>
#include <vector>

#ifdef _MSC_VER
    #define NOINLINE __declspec(noinline)
#else
    #define NOINLINE __attribute__((noinline))
#endif
static std::size_t allocations = 0;
void* operator new(std::size_t size)
{
    ++allocations;
    if (auto* p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    ::operator delete(p);
}

// Exact old sizing, temporary-buffer, then owning-string conversion, on valid UTF-8 inputs only.
template <class Dest, class Char>
NOINLINE Dest legacy(std::basic_string_view<Char> text)
{
    auto* input = text.data();
    std::mbstate_t state{};
    if constexpr (std::is_same_v<Char, char>)
    {
        const auto count = std::mbsrtowcs(nullptr, &input, 0, &state);
        if (count == std::size_t(-1))
            throw std::invalid_argument("Locale cannot represent benchmark input");
        auto buffer = std::make_unique<wchar_t[]>(count + 1);
        std::mbsrtowcs(buffer.get(), &input, count + 1, &state);
        return Dest(buffer.get());
    }
    else
    {
        const auto count = std::wcsrtombs(nullptr, &input, 0, &state);
        if (count == std::size_t(-1))
            throw std::invalid_argument("Locale cannot represent benchmark input");
        auto buffer = std::make_unique<char[]>(count + 1);
        std::wcsrtombs(buffer.get(), &input, count + 1, &state);
        return Dest(buffer.get());
    }
}
template <class Dest, class Char>
NOINLINE Dest current(std::basic_string_view<Char> text)
{
    return LLUtils::StringUtility::ConvertString<Dest>(text);
}
std::size_t sink = 0;

template <class Dest, class Char>
void measure(const char* name, const std::basic_string<Char>& text, int iterations)
{
    using Function = Dest (*)(std::basic_string_view<Char>);
    const std::array<Function, 2> functions{legacy<Dest, Char>, current<Dest, Char>};
    const auto expected = functions[0](text);
    for (auto function : functions)
        if (function(text) != expected)
            std::abort();
    std::array<std::vector<double>, 2> samples;
    std::array<std::size_t, 2> counts{};
    for (auto& sample : samples)
        sample.reserve(7);
    for (int sample = 0; sample < 7; ++sample)
    {
        // Rotate order to avoid systematically charging warmup to one implementation.
        for (int offset = 0; offset < 2; ++offset)
        {
            const int index   = (sample + offset) % 2;
            std::size_t sum   = 0;
            const auto before = allocations;
            const auto start  = std::chrono::steady_clock::now();
            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                const auto result = functions[index](text);
                sum += result.size() + static_cast<unsigned>(result.front()) + static_cast<unsigned>(result.back());
            }
            const auto end = std::chrono::steady_clock::now();
            counts[index]  = (allocations - before) / static_cast<std::size_t>(iterations);
            sink += sum;
            samples[index].push_back(std::chrono::duration<double, std::nano>(end - start).count() / iterations);
        }
    }
    for (auto& sample : samples)
        std::sort(sample.begin(), sample.end());
    const auto result = current<Dest, Char>(text);
    std::printf("%s,%zu,%.1f,%.1f,%zu,%zu,%zu,%zu\n", name, text.size(), samples[0][3], samples[1][3], counts[0],
                counts[1], result.size(), result.capacity());
}
template <class Char>
NOINLINE std::size_t oldCopy(Char* dest, const Char* source, std::size_t, std::size_t capacity)
{
    if constexpr (std::is_same_v<Char, char>)
        std::strncpy(dest, source, capacity);
    else
        std::wcsncpy(dest, source, capacity);
    return 0;
}
template <class Char>
NOINLINE std::size_t newCopy(Char* dest, const Char* source, std::size_t size, std::size_t capacity)
{
    return LLUtils::StringUtility::StrCpy(dest, std::basic_string_view<Char>(source, size), capacity).written;
}
template <class Char>
void measureCopy(const char* name, const std::basic_string<Char>& text, std::size_t capacity, int iterations)
{
    using Function = std::size_t (*)(Char*, const Char*, std::size_t, std::size_t);
    const std::array<Function, 2> functions{oldCopy<Char>, newCopy<Char>};
    std::vector<Char> destination(capacity);
    std::array<std::vector<double>, 2> samples;
    for (auto& sample : samples)
        sample.reserve(7);
    for (int sample = 0; sample < 7; ++sample)
    {
        for (int offset = 0; offset < 2; ++offset)
        {
            const int index   = (sample + offset) % 2;
            std::size_t sum   = 0;
            const auto before = allocations;
            const auto start  = std::chrono::steady_clock::now();
            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                sum += functions[index](destination.data(), text.data(), text.size(), capacity);
                sum += static_cast<unsigned>(
                    destination[static_cast<std::size_t>(iteration) % (std::min) (text.size(), capacity - 1)]);
            }
            const auto end = std::chrono::steady_clock::now();
            if (allocations != before)
                std::abort();
            sink += sum;
            samples[index].push_back(std::chrono::duration<double, std::nano>(end - start).count() / iterations);
        }
    }
    for (auto& sample : samples)
        std::sort(sample.begin(), sample.end());
    std::printf("%s,%zu,%.1f,%.1f,0,0,0,%zu\n", name, text.size(), samples[0][3], samples[1][3], capacity);
}

NOINLINE std::vector<std::string> oldSplit(const std::string& source)
{
    std::stringstream stream(source);
    std::vector<std::string> result;
    std::string item;
    while (std::getline(stream, item, ','))
    {
        if (!item.empty())
        {
            while (!item.empty() && item.back() == '\0')
                item.pop_back();
            result.push_back(item);
        }
    }
    return result;
}
NOINLINE std::vector<std::string> newSplit(const std::string& source)
{
    return LLUtils::StringUtility::split(source, ',');
}
void measureSplit()
{
    const std::string text = std::string(64, 'a') + ',' + std::string(80, 'b') + ',' + std::string(72, 'c');
    const std::array functions{oldSplit, newSplit};
    if (oldSplit(text) != newSplit(text))
        std::abort();
    std::array<std::vector<double>, 2> samples;
    std::array<std::size_t, 2> counts{};
    for (auto& sample : samples)
        sample.reserve(7);
    for (int sample = 0; sample < 7; ++sample)
    {
        for (int offset = 0; offset < 2; ++offset)
        {
            const auto index  = (sample + offset) % 2;
            const auto before = allocations;
            const auto start  = std::chrono::steady_clock::now();
            for (int iteration = 0; iteration < 50000; ++iteration)
            {
                const auto result = functions[index](text);
                sink += result.size() + result[0].size() + result[2].size();
            }
            const auto end = std::chrono::steady_clock::now();
            counts[index]  = (allocations - before) / 50000;
            samples[index].push_back(std::chrono::duration<double, std::nano>(end - start).count() / 50000);
        }
    }
    for (auto& sample : samples)
        std::sort(sample.begin(), sample.end());
    std::printf("split 64/80/72,%zu,%.1f,%.1f,%zu,%zu,3,0\n", text.size(), samples[0][3], samples[1][3], counts[0],
                counts[1]);
    std::string owned(256, 'x');
    const auto before   = allocations;
    const auto* storage = owned.data();
    const auto moved    = LLUtils::StringUtility::ConvertString<std::string>(std::move(owned));
    if (allocations != before || moved.data() != storage)
        std::abort();
}
NOINLINE std::string oldLower(const std::string& value)
{
    auto result = value;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}
NOINLINE std::string newLower(const std::string& value)
{
    return LLUtils::StringUtility::ToLower(value);
}
template <class Function>
void measureUtility(const char* name, const std::string& text, Function old, Function current)
{
    const std::array functions{old, current};
    if (old(text) != current(text))
        std::abort();
    std::array<std::vector<double>, 2> samples;
    std::array<std::size_t, 2> counts{};
    for (auto& sample : samples)
        sample.reserve(7);
    for (int sample = 0; sample < 7; ++sample)
    {
        for (int offset = 0; offset < 2; ++offset)
        {
            const int index   = (sample + offset) % 2;
            const auto before = allocations;
            std::size_t sum   = 0;
            const auto start  = std::chrono::steady_clock::now();
            for (int iteration = 0; iteration < 100000; ++iteration)
            {
                const auto result = functions[index](text);
                sum += result.size() + static_cast<unsigned>(result.front());
            }
            const auto end = std::chrono::steady_clock::now();
            counts[index]  = (allocations - before) / 100000;
            sink += sum;
            samples[index].push_back(std::chrono::duration<double, std::nano>(end - start).count() / 100000);
        }
    }
    for (auto& sample : samples)
        std::sort(sample.begin(), sample.end());
    std::printf("%s,%zu,%.1f,%.1f,%zu,%zu,0,0\n", name, text.size(), samples[0][3], samples[1][3], counts[0],
                counts[1]);
}

int main()
{
#ifdef _WIN32
    if (!std::setlocale(LC_CTYPE, ".UTF-8"))
        return 2;
#else
    if (!std::setlocale(LC_CTYPE, "C.UTF-8"))
        return 2;
#endif
    std::puts("case,input_units,legacy_ns,current_ns,legacy_allocs,current_allocs,output_units,capacity");
    for (const auto length : {32, 4096})
    {
        measure<std::wstring>("ASCII UTF8 to wide", std::string(length, 'x'), length == 32 ? 50000 : 5000);
        measure<std::string>("ASCII wide to UTF8", std::wstring(length, L'x'), length == 32 ? 50000 : 5000);
    }
    for (const int repeats : {4, 512})
    {
        std::string text;
        std::wstring wide;
        for (int i = 0; i < repeats; ++i)
        {
            text += "a\xc3\xa9\xd7\x90\xf0\x9f\x93\xb7";
            wide += L"a\u00e9\u05d0\U0001f4f7";
        }
        measure<std::wstring>("Unicode UTF8 to wide", text, repeats == 4 ? 50000 : 5000);
        measure<std::string>("Unicode wide to UTF8", wide, repeats == 4 ? 50000 : 5000);
    }
    measureCopy("copy ASCII char 32", std::string(32, 'x'), 128, 100000);
    measureCopy("copy ASCII wchar 32", std::wstring(32, L'x'), 128, 100000);
    measureCopy("copy ASCII char 4096", std::string(4096, 'x'), 4097, 10000);
    measureCopy("copy ASCII wchar 4096", std::wstring(4096, L'x'), 4097, 10000);
    std::string text;
    std::wstring wide;
    for (int i = 0; i < 40; ++i)
    {
        text += "a\xc3\xa9\xf0\x9f\x93\xb7";
        wide += L"a\u00e9\U0001f4f7";
    }
    measureCopy("copy truncated UTF8", text, 128, 100000);
    measureCopy("copy truncated wide", wide, 64, 100000);
    measureSplit();
    measureUtility("lower short", std::string(32, 'A'), oldLower, newLower);
    measureUtility("lower long", std::string(4096, 'A'), oldLower, newLower);
    std::printf("checksum,%zu\n", sink);
}
