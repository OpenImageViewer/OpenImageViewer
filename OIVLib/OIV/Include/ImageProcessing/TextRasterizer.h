#pragma once

#include <LLUtils/StringDefs.h>
#include <Image.h>
#include <LLUtils/BitFlags.h>
#include <FreeTypeWrapper/FreeTypeConnector.h>

namespace FreeType
{
    class FreeTypeConnector;
}

namespace OIV
{
    // Existing font and layout options used by CPU metrics and bitmap generation.
    struct CreateTextParams
    {
        // Native font file path passed to the existing font connector.
        LLUtils::native_string_type fontPath;
        // Text content in the platform native character encoding.
        LLUtils::native_string_type text;
        // Wrapping limit in pixels, following the existing zero/default policy.
        int32_t maxWidth;
        // Outline thickness in pixels.
        uint32_t outlineWidth;
        // Foreground RGBA color.
        LLUtils::Color textColor;
        // Bitmap background RGBA color.
        LLUtils::Color backgroundColor;
        // Configured outline color, preserving the existing producer policy.
        LLUtils::Color outlineColor;
        // Font size in points, converted using the selected DPI.
        uint16_t fontSize;
        // Horizontal dots per inch, zero resolves to the existing default of 96.
        uint16_t DPIx;
        // Vertical dots per inch, zero resolves to the existing default of 96.
        uint16_t DPIy;

        // Whether inline text formatting commands are interpreted.
        bool useMetaText;
        // Whether the font connector applies bidirectional text handling.
        bool bidirectional;
        // Whether line endings retain the existing fixed-width layout rule.
        bool lineEndFixedWidth;
        // Existing antialiasing and glyph rasterization mode.
        FreeType::RenderMode renderMode;
    };

    // Cached layout summary, available without generating bitmap pixels.
    struct TextMetrics
    {
        // Measured row height in pixels.
        uint32_t rowHeight;
        // Number of measured text rows.
        uint32_t totalRows;
        // Measured bitmap bounds in pixels.
        LLUtils::PointI32 pixelSize{};
    };

    // CPU text producer with independent layout and bitmap invalidation.
    class TextRasterizer
    {
        // Independent invalidation lets hidden labels measure without rasterizing.
        enum class DirtyFlags : uint32_t
        {
            // No pending producer work.
            None = 0,
            // Layout must be measured again.
            Metrics = 1 << 0,
            // CPU bitmap must be prepared again.
            Bitmap = 1 << 1,
            // Both layout and pixels are invalid.
            All = Metrics | Bitmap
        };
        LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS_IN_CLASS(DirtyFlags);

      public:

        explicit TextRasterizer(FreeType::FreeTypeConnector* freeType);

#pragma region Text rendering
        void SetText(const LLUtils::native_string_type& text)
        {
            if (fTextOptionsCurrent.text != text)
            {
                // Text content in the platform native character encoding.
                fTextOptionsCurrent.text = text;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }
        void SetDPI(uint16_t dpix, uint16_t dpiy)
        {
            if (fTextOptionsCurrent.DPIx != dpix || fTextOptionsCurrent.DPIy != dpiy)
            {
                fTextOptionsCurrent.DPIx = dpix;
                fTextOptionsCurrent.DPIy = dpiy;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetFontPath(const LLUtils::native_string_type& sFontPath)
        {
            if (fTextOptionsCurrent.fontPath != sFontPath)
            {
                fTextOptionsCurrent.fontPath = sFontPath;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetFontSize(uint16_t fontSize)
        {
            if (fTextOptionsCurrent.fontSize != fontSize)
            {
                // Font size in points, converted using the selected DPI.
                fTextOptionsCurrent.fontSize = fontSize;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }
        void SetOutlineWidth(uint16_t outlineWidth)
        {
            if (fTextOptionsCurrent.outlineWidth != outlineWidth)
            {
                // Outline thickness in pixels.
                fTextOptionsCurrent.outlineWidth = outlineWidth;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetBackgroundColor(LLUtils::Color color)
        {
            if (fTextOptionsCurrent.backgroundColor != color)
            {
                fTextOptionsCurrent.backgroundColor = color;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetTextColor(LLUtils::Color color)
        {
            if (fTextOptionsCurrent.textColor != color)
            {
                fTextOptionsCurrent.textColor = color;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetUseMetaText(bool useMetaText)
        {
            if (fTextOptionsCurrent.useMetaText != useMetaText)
            {
                // Whether inline text formatting commands are interpreted.
                fTextOptionsCurrent.useMetaText = useMetaText;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetMaxWidth(int32_t maxWidth)
        {
            if (fTextOptionsCurrent.maxWidth != maxWidth)
            {
                // Wrapping limit in pixels, following the existing zero/default policy.
                fTextOptionsCurrent.maxWidth = maxWidth;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetTextRenderMode(FreeType::RenderMode renderMode)
        {
            if (fTextOptionsCurrent.renderMode != renderMode)
            {
                // Existing antialiasing and glyph rasterization mode.
                fTextOptionsCurrent.renderMode = renderMode;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetLineEndFixedWidth(bool lineEndFixedWidth)
        {
            if (fTextOptionsCurrent.lineEndFixedWidth != lineEndFixedWidth)
            {
                // Whether line endings retain the existing fixed-width layout rule.
                fTextOptionsCurrent.lineEndFixedWidth = lineEndFixedWidth;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        TextMetrics GetMetrics();
        void UpdateTextMetrics();
        // Explicitly prepare dirty pixels, retaining successful empty output.
        IMCodec::ImageSharedPtr GetBitmap();
        bool IsBitmapDirty() const { return fDirtyFlags.test(DirtyFlags::Bitmap); }

#pragma endregion Text display

      private:

        // Current text layout and pixel options, changed only through invalidating setters.
        CreateTextParams fTextOptionsCurrent{};

        FreeType::TextCreateParams GetCreateParams();
        IMCodec::ImageSharedPtr CreateText();
        // Layout and pixel work still required by the current options.
        LLUtils::BitFlags<DirtyFlags> fDirtyFlags{};
        // Reused layout measurements consumed by bitmap production.
        FreeType::TextMetrics fCachedTextMetrics{};
        // Borrowed font connector, which must outlive this producer.
        FreeType::FreeTypeConnector* fFreeType{};
        // Last successful CPU bitmap, empty for successful empty text.
        IMCodec::ImageSharedPtr fBitmap;
    };

}  // namespace OIV
