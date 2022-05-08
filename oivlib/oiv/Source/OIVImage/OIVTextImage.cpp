#include <OIVImage/OIVTextImage.h>
#include "../FreeTypeHelper.h"
#include <defs.h>
#include "../ImageUtil.h"

namespace OIV
{

    ResultCode OIVTextImage::CreateText()
    {
#if OIV_BUILD_FREETYPE == 1

        using namespace FreeType;

        FreeType::TextCreateParams createParams {};

        createParams.backgroundColor = fTextOptionsCurrent.backgroundColor;
        createParams.fontPath = fTextOptionsCurrent.fontPath;
        createParams.fontSize = fTextOptionsCurrent.fontSize;
        createParams.outlineColor = { 0,0,0,255 };// request.outlineColor;
        createParams.outlineWidth = fTextOptionsCurrent.outlineWidth;
        createParams.text = fTextOptionsCurrent.text;
        createParams.renderMode = RenderMode::Antialiased;
        createParams.DPIx = fTextOptionsCurrent.DPIx == 0 ? 96 : fTextOptionsCurrent.DPIx;
        createParams.DPIy = fTextOptionsCurrent.DPIy == 0 ? 96 : fTextOptionsCurrent.DPIy;
        createParams.flags |= fTextOptionsCurrent.useMetaText ? TextCreateFlags::UseMetaText : TextCreateFlags::None;
        createParams.flags |= fTextOptionsCurrent.bidirectional ? TextCreateFlags::Bidirectional : TextCreateFlags::None;

        IMCodec::ImageSharedPtr imageText = FreeTypeHelper::CreateRGBAText(createParams);

        if (imageText != nullptr)
        {
            if (imageText->GetOriginalTexelFormat() != IMCodec::TexelFormat::I_B8_G8_R8_A8 && imageText->GetOriginalTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
                imageText = IMUtil::ImageUtil::Convert(imageText, IMCodec::TexelFormat::I_B8_G8_R8_A8);

            //this->fTextOptionsCached = this->fTextOptionsCurrent;

            SetUnderlyingImage(imageText);
            fIsDirty = false;
            
            return RC_Success;
        }

        return RC_InvalidParameters;
#endif

    }
}