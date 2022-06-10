#pragma once
#include "OIVBaseImage.h"

namespace OIV
{
    struct CreateTextParams
    {
        std::wstring fontPath;
        std::wstring text;
        uint16_t fontSize;
        LLUtils::Color textColor;
        LLUtils::Color backgroundColor;
        LLUtils::Color outlineColor;
        uint32_t outlineWidth;
        uint16_t DPIx;
        uint16_t DPIy;
        bool useMetaText;
        bool bidirectional;
    };

    

    class OIVTextImage : public OIVBaseImage
    {
    public:
        OIVTextImage() : OIVBaseImage(ImageSource::InternalText) 
        {
            fTextOptionsCurrent.bidirectional = true;
            fTextOptionsCurrent.useMetaText = true;
        }
    
#pragma region Text rendering
        void SetText(const std::wstring& text)
        {
            if (fTextOptionsCurrent.text != text)
            {
                fTextOptionsCurrent.text = text;
                fIsDirty = true;
            }
        }
        void SetDPI(uint16_t dpix, uint16_t dpiy)
        {
            if (fTextOptionsCurrent.DPIx != dpix || fTextOptionsCurrent.DPIy != dpiy)
            {
                fTextOptionsCurrent.DPIx = dpix;
                fTextOptionsCurrent.DPIy = dpiy;
                fIsDirty = true;
            }
        }

        void  SetFontPath(const std::wstring& sFontPath)
        {
            if (fTextOptionsCurrent.fontPath != sFontPath)
            {
                fTextOptionsCurrent.fontPath = sFontPath;
                fIsDirty = true;
            }
        }

        void SetFontSize(uint16_t fontSize)
        {
            if (fTextOptionsCurrent.fontSize != fontSize)
            {
                fTextOptionsCurrent.fontSize = fontSize;
                fIsDirty = true;
            }

        }
        void SetOutlineWidth(uint16_t outlineWidth) 
        {
            if (fTextOptionsCurrent.outlineWidth != outlineWidth)
            {
                fTextOptionsCurrent.outlineWidth = outlineWidth;
                fIsDirty = true;
            }
        }
  
        
        void SetBackgroundColor(LLUtils::Color color)
        {
            if (fTextOptionsCurrent.backgroundColor != color)
            {
                fTextOptionsCurrent.backgroundColor = color;
                fIsDirty = true;
            }
        }

        void SetTextColor(LLUtils::Color color)
        {
            if (fTextOptionsCurrent.textColor != color)
            {
                fTextOptionsCurrent.textColor = color;
                fIsDirty = true;
            }
        }

        void SetUseMetaText(bool useMetaText)
        {
            if (fTextOptionsCurrent.useMetaText != useMetaText)
            {
                fTextOptionsCurrent.useMetaText = useMetaText;
                fIsDirty = true;
            }
        }

        void Create()
        {
            CreateText();
        }

    
#pragma endregion Text display

#pragma region Text rendering



        /*OIV_CMD_ImageProperties_Request fImageProperiesCurrent;
        OIV_CMD_ImageProperties_Request fImageProperiesCached;*/

        CreateTextParams fTextOptionsCached{};
        CreateTextParams fTextOptionsCurrent{};
        
    protected:

        void PerformPreRender() override 
        { 
            OIVBaseImage::PerformPreRender();
            
            if (fIsDirty == true)
                CreateText();
        };

        bool PerformIsDirty() const override
        {
            return fIsDirty || OIVBaseImage::PerformIsDirty();
        };

    private:
       ResultCode CreateText();
       bool fIsDirty = true;
    };


    using OIVTextImageUniquePtr = std::unique_ptr<OIVTextImage>;
    using OIVTextImageSharedPtr = std::shared_ptr<OIVTextImage>;

}