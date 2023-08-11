#include <OIVImage/OIVTextImage.h>
#include "../FreeTypeHelper.h"
#include <defs.h>
#include "../ImageUtil.h"

namespace OIV
{

    
    OIVTextImage::OIVTextImage(ImageSource imageSource) : OIVBaseImage(imageSource)

    {
        fTextOptionsCurrent.bidirectional = true;
        fTextOptionsCurrent.useMetaText = true;
    }

    OIVTextImage::OIVTextImage() : OIVTextImage(ImageSource::InternalText)
    {

    }
    

    FreeType::TextCreateParams OIVTextImage::GetCreateParams()
    {
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
        createParams.maxWidthPx = fTextOptionsCurrent.maxWidth;
        return createParams;
    }
    

    void OIVTextImage::UpdateTextMetrics()
    {
        using namespace FreeType;

        if (fDirtyFlags.test(DirtyFlags::Metrics))
        {
            FreeTypeConnector::GetSingleton().MeasureText({ GetCreateParams() }, fCachedTextMetrics);
            fDirtyFlags.clear(DirtyFlags::Metrics);
        }
    }


    TextMetrics OIVTextImage::GetMetrics()
    {
        UpdateTextMetrics();
        return { fCachedTextMetrics.rowHeight, fCachedTextMetrics.totalRows };
    }

    

    IMCodec::ImageSharedPtr OIVTextImage::CreateText()
    {
#if OIV_BUILD_FREETYPE == 1

        IMCodec::ImageSharedPtr imageText = FreeType::FreeTypeHelper::CreateRGBAText(GetCreateParams(), &fCachedTextMetrics);

        if (imageText != nullptr)
        {
            if (imageText->GetTexelFormat() != IMCodec::TexelFormat::I_B8_G8_R8_A8 && imageText->GetTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
                imageText = IMUtil::ImageUtil::Convert(imageText, IMCodec::TexelFormat::I_B8_G8_R8_A8);
        }

        return imageText;
#endif

        return nullptr;

    }
}