#pragma once
#include "OIVBaseImage.h"
namespace OIV
{
    struct CreateTextParams
    {

        bool operator == (const CreateTextParams& rhs) const
        {
            return  !
                (
                    false
                    || text != rhs.text
                    || fontPath != rhs.fontPath
                    || fontSize != rhs.fontSize
                    || backgroundColor != rhs.backgroundColor
                    || outlineWidth != rhs.outlineWidth
                    || outlineColor != rhs.outlineColor
                    || DPIx != rhs.DPIx
                    || DPIy != rhs.DPIy
                    || renderMode != rhs.renderMode
                    );
        }

        bool operator != (const CreateTextParams& rhs) const
        {
            return !(*this == rhs);
        }

        OIVString text;
        OIVString fontPath;
        uint16_t fontSize;
        uint32_t backgroundColor;
        uint8_t outlineWidth;
        uint8_t outlineColor;
        uint16_t DPIx;
        uint16_t DPIy;
        OIV_PROP_CreateText_Mode renderMode;
    };


    class OIVTextImage : public OIVBaseImage
    {
    public:
        CreateTextParams& GetTextOptions() { return  fTextOptionsCurrent; }
        ResultCode DoUpdate() override;

    public: //const methods
        CreateTextParams fTextOptionsCached = {};
        CreateTextParams fTextOptionsCurrent = {};

    private:

        ResultCode CreateText();
    };


    using OIVTextImageUniquePtr = std::unique_ptr<OIVTextImage>;

}