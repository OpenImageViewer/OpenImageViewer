#include <catch2/catch_test_macros.hpp>

#include <OIVShared/AtomicFile.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <thread>
#include <exception>

namespace
{
    struct TemporaryDirectory
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     ("OIVShared-atomic-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        TemporaryDirectory() { std::filesystem::create_directories(root); }
        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    };

    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
}  // namespace

TEST_CASE("Atomic files remain complete under concurrent writers", "[Shared][AtomicFile]")
{
    TemporaryDirectory directory;
    const auto path = directory.root / "atomic";
    const std::string first(8192, 'a');
    const std::string second(8192, 'b');
    std::atomic_uint completed{};
    const auto write = [&](const std::string& content)
    {
        try
        {
            OIV::WriteFileAtomically(path, std::as_bytes(std::span(content.data(), content.size())));
            ++completed;
        }
        catch (const std::exception&)
        {
            // Concurrent cache replacements may lose a Windows rename race. Readers still need a complete file.
        }
    };
    {
        std::jthread a([&] { write(first); });
        std::jthread b([&] { write(second); });
    }
    REQUIRE(completed > 0);
    const auto content = Read(path);
    REQUIRE((content == first || content == second));
    REQUIRE(std::ranges::none_of(std::filesystem::directory_iterator(directory.root),
                                 [](const auto& entry) { return entry.path().extension() == ".tmp"; }));
}

TEST_CASE("Atomic replacement failures preserve existing directory contents", "[Shared][AtomicFile]")
{
    TemporaryDirectory directory;
    const auto destination = directory.root / "directory";
    std::filesystem::create_directory(destination);
    std::ofstream(destination / "keep") << "keep";
    const std::string content = "replacement";
    REQUIRE_THROWS(OIV::WriteFileAtomically(destination, std::as_bytes(std::span(content.data(), content.size()))));
    REQUIRE(Read(destination / "keep") == "keep");
}
