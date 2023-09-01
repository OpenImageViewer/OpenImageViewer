#pragma once
#include "OIVBaseImage.h"
#include <LLUtils/BitFlags.h>
#include <LLUtils/Templates.h>
#include <FreeTypeWrapper/FreeTypeConnector.h>

namespace FreeType
{
    class FreeTypeConnector;
}


namespace OIV
{
    struct CreateTextParams
    {
        std::wstring fontPath;
        std::wstring text;
        int32_t maxWidth;
        uint32_t outlineWidth;
        LLUtils::Color textColor;
        LLUtils::Color backgroundColor;
        LLUtils::Color outlineColor;
        uint16_t fontSize;
        uint16_t DPIx;
        uint16_t DPIy;

        bool useMetaText;
        bool bidirectional;
        bool lineEndFixedWidth;
        FreeType::RenderMode renderMode;
    };

    
    struct TextMetrics
    {
        uint32_t rowHeight;
        uint32_t totalRows;
    };


    class OIVTextImage : public OIVBaseImage
    {
        enum class DirtyFlags : uint32_t
        {
              None      = 0
            , Metrics   = 1 << 0
            , Bitmap    = 1 << 1
            , All       = LLUtils::GetMaxBitsMask<uint32_t>()
        };
        LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS_IN_CLASS(DirtyFlags);

    public:

        OIVTextImage(ImageSource imageSource, FreeType::FreeTypeConnector* freeType);

        OIVTextImage(FreeType::FreeTypeConnector* freeType);
        
    
#pragma region Text rendering
        void SetText(const std::wstring& text)
        {
            if (fTextOptionsCurrent.text != text)
            {
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

        void  SetFontPath(const std::wstring& sFontPath)
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
                fTextOptionsCurrent.fontSize = fontSize;
                fDirtyFlags.set(DirtyFlags::All);
            }

        }
        void SetOutlineWidth(uint16_t outlineWidth) 
        {
            if (fTextOptionsCurrent.outlineWidth != outlineWidth)
            {
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
                fTextOptionsCurrent.useMetaText = useMetaText;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetMaxWidth(int32_t maxWidth)
        {
            if (fTextOptionsCurrent.maxWidth != maxWidth)
            {
                fTextOptionsCurrent.maxWidth = maxWidth;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetTextRenderMode(FreeType::RenderMode renderMode)
        {
            if (fTextOptionsCurrent.renderMode != renderMode)
            {
                fTextOptionsCurrent.renderMode = renderMode;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }

        void SetLineEndFixedWidth(bool lineEndFixedWidth)
        {
            if (fTextOptionsCurrent.lineEndFixedWidth != lineEndFixedWidth)
            {
                fTextOptionsCurrent.lineEndFixedWidth = lineEndFixedWidth;
                fDirtyFlags.set(DirtyFlags::All);
            }
        }


        TextMetrics GetMetrics();
        void UpdateTextMetrics();
        void Create()
        {
            UpdateBitmap();
        }

    
#pragma endregion Text display

#pragma region Text rendering



        /*OIV_CMD_ImageProperties_Request fImageProperiesCurrent;
        OIV_CMD_ImageProperties_Request fImageProperiesCached;*/

        CreateTextParams fTextOptionsCached{};
        CreateTextParams fTextOptionsCurrent{};
        
    protected:

        void UpdateBitmap()
        {
            UpdateTextMetrics();
            if (fDirtyFlags.test(DirtyFlags::Bitmap))
            {
                auto textImage = CreateText();
                if (textImage != nullptr)
                {
                    SetUnderlyingImage(textImage);
                    fDirtyFlags.clear(DirtyFlags::Bitmap);
                }
            }

        }

        void PerformPreRender() override 
        { 
            OIVBaseImage::PerformPreRender();
            UpdateBitmap();

        };

        bool PerformIsDirty() const override
        {
            return fDirtyFlags.testAny(DirtyFlags::All) || OIVBaseImage::PerformIsDirty();
        };

    private:

       FreeType::TextCreateParams GetCreateParams();
       IMCodec::ImageSharedPtr CreateText();
       LLUtils::BitFlags<DirtyFlags> fDirtyFlags{};
       FreeType::TextMetrics fCachedTextMetrics;
       FreeType::FreeTypeConnector* fFreeType{};
        
    };


    using OIVTextImageUniquePtr = std::unique_ptr<OIVTextImage>;
    using OIVTextImageSharedPtr = std::shared_ptr<OIVTextImage>;

}