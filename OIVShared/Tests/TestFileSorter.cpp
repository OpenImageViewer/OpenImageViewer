#include <catch2/catch_test_macros.hpp>

#include <OIVShared/FileSorter.h>

#include <filesystem>

TEST_CASE("FileSorter orders files by name and extension", "[Shared]")
{
    OIV::FileSorter sorter;
    const auto basePath = std::filesystem::path{} / "images";

    const LLUtils::native_string_type png = (basePath / "b.png").native();
    const LLUtils::native_string_type jpg = (basePath / "a.jpg").native();

    sorter.SetSortType(OIV::FileSorter::SortType::Name);
    REQUIRE(sorter(jpg, png));

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    REQUIRE(sorter(jpg, png));
}
