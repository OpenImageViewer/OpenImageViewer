#pragma once
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace OIV
{
    enum class ResizeSizeMode
    {
        Absolute,
        Relative
    };
    enum class ResizeUnits
    {
        Pixels,
        Inches,
        Centimeters
    };
    struct ResizeFormDimensions
    {
        double width{};
        double height{};
    };

    inline std::string FormatResizeDimension(double value)
    {
        return std::format("{:.2f}", value);
    }

    // Store the last explicit input in its own units. Display rounding and mode changes never replace it.
    struct ResizeSizeBasis
    {
        ResizeSizeMode mode = ResizeSizeMode::Absolute;
        ResizeUnits units   = ResizeUnits::Pixels;
        double width{};
        double height{};

        static constexpr double PixelsPerUnit(ResizeUnits units, double dpi)
        {
            return units == ResizeUnits::Pixels ? 1.0 : units == ResizeUnits::Inches ? dpi : dpi / 2.54;
        }
        std::optional<ResizeFormDimensions> Pixels(ResizeFormDimensions original, double dpi) const
        {
            const double scale                = PixelsPerUnit(units, dpi);
            const ResizeFormDimensions pixels = mode == ResizeSizeMode::Relative
                                                    ? ResizeFormDimensions{original.width * width / 100.0,
                                                                           original.height * height / 100.0}
                                                    : ResizeFormDimensions{width * scale, height * scale};
            return Valid(pixels) ? std::optional{pixels} : std::nullopt;
        }
        std::optional<ResizeFormDimensions> Dimensions(ResizeSizeMode displayMode, ResizeUnits displayUnits,
                                                       ResizeFormDimensions original, double dpi) const
        {
            auto result = Pixels(original, dpi);
            if (result)
            {
                if (displayMode == ResizeSizeMode::Relative)
                    *result = {result->width / original.width * 100.0, result->height / original.height * 100.0};
                else
                {
                    const double scale = PixelsPerUnit(displayUnits, dpi);
                    *result            = {result->width / scale, result->height / scale};
                }
                if (!Valid(*result))
                    result.reset();
            }
            return result;
        }
        // A DPI edit changes pixels while preserving the current physical dimensions, even for a pixel basis.
        std::optional<ResizeSizeBasis> ForDPIEdit(ResizeSizeMode displayMode, ResizeUnits displayUnits,
                                                  ResizeFormDimensions original, double oldDpi) const
        {
            if (displayMode != ResizeSizeMode::Absolute || displayUnits == ResizeUnits::Pixels ||
                (mode == displayMode && units == displayUnits))
                return *this;
            const auto size = Dimensions(displayMode, displayUnits, original, oldDpi);
            return size ? std::optional{ResizeSizeBasis{displayMode, displayUnits, size->width, size->height}}
                        : std::nullopt;
        }
        // Parse only the edited dimension. An unchanged two-decimal label retains the exact stored basis.
        std::optional<ResizeSizeBasis> Edited(std::string_view text, std::string_view displayed, bool heightEdited,
                                              ResizeSizeMode displayMode, ResizeUnits displayUnits, bool preserve,
                                              ResizeFormDimensions original, double dpi) const
        {
            if (text == displayed)
                return *this;
            double value{};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value) ||
                value <= 0)
                return std::nullopt;
            return WithDimension(value, heightEdited, displayMode, displayUnits, preserve, original, dpi);
        }
        std::optional<ResizeSizeBasis> WithDimension(double value, bool heightEdited, ResizeSizeMode displayMode,
                                                     ResizeUnits displayUnits, bool preserve,
                                                     ResizeFormDimensions original, double dpi) const
        {
            if (!std::isfinite(value) || value <= 0)
                return std::nullopt;
            auto dimensions = Dimensions(displayMode, displayUnits, original, dpi);
            if (!dimensions)
                return std::nullopt;
            if (heightEdited)
                dimensions->height = value;
            else
                dimensions->width = value;
            if (preserve)
            {
                if (displayMode == ResizeSizeMode::Relative)
                    dimensions->width = dimensions->height = value;
                else if (heightEdited)
                    dimensions->width = value * original.width / original.height;
                else
                    dimensions->height = value * original.height / original.width;
            }
            const ResizeSizeBasis result{displayMode, displayUnits, dimensions->width, dimensions->height};
            return result.Pixels(original, dpi) ? std::optional{result} : std::nullopt;
        }
        std::optional<ResizeSizeBasis> Preserved(bool heightEdited, ResizeFormDimensions original, double dpi) const
        {
            auto pixels = Pixels(original, dpi);
            if (pixels)
            {
                if (heightEdited)
                    pixels->width = pixels->height * original.width / original.height;
                else
                    pixels->height = pixels->width * original.height / original.width;
            }
            return pixels && Valid(*pixels)
                       ? std::optional{ResizeSizeBasis{ResizeSizeMode::Absolute, ResizeUnits::Pixels, pixels->width,
                                                       pixels->height}}
                       : std::nullopt;
        }

      private:

        static bool Valid(ResizeFormDimensions size)
        {
            return std::isfinite(size.width) && std::isfinite(size.height) && size.width > 0 && size.height > 0;
        }
    };
}  // namespace OIV
