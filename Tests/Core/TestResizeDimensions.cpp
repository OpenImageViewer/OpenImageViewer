#include "UI/ResizeDimensions.h"
#include <ImageProcessing/ImageResize.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>

using namespace OIV;

TEST_CASE("Resize dimensions display two decimals without changing the basis", "[ResizeForm]")
{
    const ResizeFormDimensions original{130, 130};
    ResizeSizeBasis basis{ResizeSizeMode::Absolute, ResizeUnits::Pixels, 130, 130};
    CHECK(FormatResizeDimension(130) == "130.00");
    CHECK(FormatResizeDimension(1.23456789) == "1.23");
    CHECK(FormatResizeDimension(0.001) == "0.00");
    for (int repeat = 0; repeat < 100; ++repeat)
        for (auto unit : std::array{ResizeUnits::Inches, ResizeUnits::Centimeters, ResizeUnits::Pixels})
        {
            const auto display = basis.Dimensions(ResizeSizeMode::Absolute, unit, original, 96);
            REQUIRE(display);
            const auto label     = FormatResizeDimension(display->width);
            const auto unchanged = basis.Edited(label, label, false, ResizeSizeMode::Absolute, unit, true, original,
                                                96);
            REQUIRE(unchanged);
            basis = *unchanged;
        }
    CHECK(basis.width == 130);
    CHECK(basis.height == 130);
    CHECK(basis.units == ResizeUnits::Pixels);
    CHECK(basis.Pixels(original, 96)->width == 130);
}

TEST_CASE("Resize explicit input retains precision and the untouched dimension", "[ResizeForm]")
{
    const ResizeFormDimensions original{2000, 1000};
    const ResizeSizeBasis basis{ResizeSizeMode::Absolute, ResizeUnits::Pixels, 130, 123.456789};
    const auto edited = basis.Edited("1.23456789", "1.35", false, ResizeSizeMode::Absolute, ResizeUnits::Inches, false,
                                     original, 96);
    REQUIRE(edited);
    CHECK(edited->width == 1.23456789);
    CHECK(edited->Pixels(original, 96)->width == Catch::Approx(1.23456789 * 96));
    CHECK(edited->Pixels(original, 96)->height == Catch::Approx(123.456789));
    CHECK(edited->Pixels(original, 192)->width == Catch::Approx(1.23456789 * 192));
    // Previewing a candidate never mutates the committed basis; cancel keeps the original exact data.
    CHECK(basis.width == 130);
    CHECK(basis.height == 123.456789);
}

TEST_CASE("Resize preserve links relative percentages for either edited dimension", "[ResizeForm]")
{
    const ResizeFormDimensions original{2000, 1000};
    const ResizeSizeBasis basis{ResizeSizeMode::Relative, ResizeUnits::Pixels, 100, 100};
    for (bool heightEdited : {false, true})
    {
        const auto edited = basis.Edited("50.123456789", "100.00", heightEdited, ResizeSizeMode::Relative,
                                         ResizeUnits::Pixels, true, original, 96);
        REQUIRE(edited);
        CHECK(edited->width == 50.123456789);
        CHECK(edited->height == 50.123456789);
        CHECK(edited->Pixels(original, 96)->width == Catch::Approx(1002.46913578));
        CHECK(edited->Pixels(original, 96)->height == Catch::Approx(501.23456789));
    }
    const auto independent = basis.Edited("60", "100.00", false, ResizeSizeMode::Relative, ResizeUnits::Pixels, false,
                                          original, 96);
    REQUIRE(independent);
    CHECK(independent->width == 60);
    CHECK(independent->height == 100);
}

TEST_CASE("Resize enabling preserve keeps the last edited axis", "[ResizeForm]")
{
    const ResizeFormDimensions original{2000, 1000};
    const ResizeSizeBasis basis{ResizeSizeMode::Relative, ResizeUnits::Pixels, 60, 80};
    const auto byWidth  = basis.Preserved(false, original, 96);
    const auto byHeight = basis.Preserved(true, original, 96);
    REQUIRE(byWidth);
    REQUIRE(byHeight);
    const auto widthPercent  = byWidth->Dimensions(ResizeSizeMode::Relative, ResizeUnits::Pixels, original, 96);
    const auto heightPercent = byHeight->Dimensions(ResizeSizeMode::Relative, ResizeUnits::Pixels, original, 96);
    REQUIRE(widthPercent);
    REQUIRE(heightPercent);
    CHECK(widthPercent->width == 60);
    CHECK(widthPercent->height == 60);
    CHECK(heightPercent->width == 80);
    CHECK(heightPercent->height == 80);
    const auto physical = byHeight->Dimensions(ResizeSizeMode::Absolute, ResizeUnits::Centimeters, original, 96);
    REQUIRE(physical);
    CHECK(physical->width == Catch::Approx(1600.0 / 96 * 2.54));
    CHECK(physical->height == Catch::Approx(800.0 / 96 * 2.54));
}

TEST_CASE("Resize invalid drafts do not replace the committed data", "[ResizeForm]")
{
    const ResizeFormDimensions original{2000, 1000};
    const ResizeSizeBasis basis{ResizeSizeMode::Absolute, ResizeUnits::Pixels, 130, 130};
    for (auto input : {"", "0", "-1", "nan", "inf", "1.2.3", "1x", "1e999"})
        CHECK_FALSE(
            basis.Edited(input, "130.00", false, ResizeSizeMode::Absolute, ResizeUnits::Pixels, true, original, 96));
    CHECK(basis.width == 130);
    CHECK(basis.height == 130);
}

TEST_CASE("Resize compares the final canvas to the original pixel dimensions", "[ResizeForm]")
{
    const LLUtils::PointI32 original{130, 130};
    const ResizeSizeBasis basis{ResizeSizeMode::Absolute, ResizeUnits::Pixels, 130.49, 130.49};
    const auto pixels = basis.Pixels({130, 130}, 96);
    REQUIRE(pixels);
    const auto rounded = ResolveImageResize(original, {.size = {static_cast<int32_t>(std::round(pixels->width)),
                                                                static_cast<int32_t>(std::round(pixels->height))}});
    REQUIRE(rounded);
    CHECK(rounded->canvas == original);
    // Preserve resolves the actual height from width; the requested height alone is not a size change.
    const auto preserved = ResolveImageResize({100, 1000}, {.size = {100, 1001}});
    REQUIRE(preserved);
    CHECK(preserved->canvas == LLUtils::PointI32{100, 1000});
    const auto changed = ResolveImageResize({100, 1000}, {.size = {101, 1000}});
    REQUIRE(changed);
    CHECK(changed->canvas != LLUtils::PointI32{100, 1000});
    const auto independent = ResolveImageResize(original,
                                                {.size = {130, 131}, .aspect = ResizeAspectRatio::DoNotPreserve});
    REQUIRE(independent);
    CHECK(independent->canvas != original);
}

TEST_CASE("Resize DPI edits retain exact physical size and update output pixels", "[ResizeForm]")
{
    const ResizeFormDimensions original{130, 260};
    const ResizeSizeBasis pixels{ResizeSizeMode::Absolute, ResizeUnits::Pixels, 130, 260};
    for (auto unit : {ResizeUnits::Inches, ResizeUnits::Centimeters})
    {
        const auto physical = pixels.ForDPIEdit(ResizeSizeMode::Absolute, unit, original, 96);
        REQUIRE(physical);
        const auto at192 = physical->Pixels(original, 192);
        REQUIRE(at192);
        CHECK(at192->width == Catch::Approx(260));
        CHECK(at192->height == Catch::Approx(520));
        const auto at300 = physical->Pixels(original, 300);
        REQUIRE(at300);
        CHECK(at300->width == Catch::Approx(406.25));
        CHECK(at300->height == Catch::Approx(812.5));
        const auto before = pixels.Dimensions(ResizeSizeMode::Absolute, unit, original, 96);
        const auto after  = physical->Dimensions(ResizeSizeMode::Absolute, unit, original, 300);
        REQUIRE(before);
        REQUIRE(after);
        CHECK(after->width == before->width);
        CHECK(after->height == before->height);
        const auto nextEdit = physical->ForDPIEdit(ResizeSizeMode::Absolute, unit, original, 300);
        REQUIRE(nextEdit);
        CHECK(nextEdit->width == physical->width);
        CHECK(nextEdit->height == physical->height);
        // Restoring the edit snapshot on cancel also restores the original exact pixel basis.
        CHECK(pixels.Pixels(original, 96)->width == 130);
        CHECK(pixels.units == ResizeUnits::Pixels);
    }
    const auto unchangedPixels = pixels.ForDPIEdit(ResizeSizeMode::Absolute, ResizeUnits::Pixels, original, 96);
    REQUIRE(unchangedPixels);
    CHECK(unchangedPixels->Pixels(original, 300)->width == 130);
    const ResizeSizeBasis relative{ResizeSizeMode::Relative, ResizeUnits::Pixels, 50, 50};
    const auto unchangedRelative = relative.ForDPIEdit(ResizeSizeMode::Relative, ResizeUnits::Inches, original, 96);
    REQUIRE(unchangedRelative);
    CHECK(unchangedRelative->Pixels(original, 300)->width == 65);
}
