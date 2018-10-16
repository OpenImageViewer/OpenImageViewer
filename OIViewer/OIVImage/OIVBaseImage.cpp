#include "OIVBaseImage.h"
#include "../OIVCommands.h"
namespace OIV
{
    OIVBaseImage::OIVBaseImage()
    {
        fImageProperties.imageHandle = ImageHandleDisplayed;
        fImageProperties.position = 0;
        fImageProperties.filterType = OIV_Filter_type::FT_None;
        fImageProperties.imageRenderMode = OIV_Image_Render_mode::IRM_MainImage;
        fImageProperties.scale = 1.0;
        fImageProperties.opacity = 1.0;

    }
    ResultCode OIVBaseImage::Display(const DisplayOptions & loadOptions)
    {

        ResultCode result = ResultCode::RC_UknownError;

        if (fDescriptor.ImageHandle != ImageHandleNull)
        {
            LLUtils::StopWatch stopWatch(true);

            result = OIVCommands::DisplayImage(fDescriptor.ImageHandle
                , static_cast<OIV_CMD_DisplayImage_Flags>(OIV_CMD_DisplayImage_Flags::DF_ApplyExifTransformation)
                , loadOptions.fUseRainbowNormalization ? OIV_PROP_Normalize_Mode::NM_Rainbow : OIV_PROP_Normalize_Mode::NM_Monochrome
            );

            fDescriptor.DisplayTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);
        }

        return result;
    }

    ResultCode OIVBaseImage::Update()
    {
        return DoUpdate();
    }

    ResultCode OIVBaseImage::DoUpdate()
    {
        ResultCode result = ResultCode::RC_UknownError;
        if (memcmp(&fImagePropertiesCached, &fImageProperties, sizeof(fImageProperties)) != 0)
        {
            result = OIVCommands::ExecuteCommand(OIV_CMD_ImageProperties, &fImageProperties, &CmdNull());
            fImagePropertiesCached = fImageProperties;
        }
        return result;
    }
   
    std::wstring OIVBaseImage::GetDescription() const
    {
        std::wstringstream ss;
        ss << fDescriptor.Width << L" X " << fDescriptor.Height << L" X "
            << fDescriptor.Bpp << L" BPP | loaded in " << std::fixed << std::setprecision(1)
            << fDescriptor.LoadTime
            << L"/" << fDescriptor.DisplayTime + fDescriptor.LoadTime << L" ms";

        return ss.str();
    }
    
    OIV_CMD_ImageProperties_Request & OIVBaseImage::GetImagePropertiesCurrent() { return fImageProperties; }

    void OIVBaseImage::FreeImage()
    {
        if (GetDescriptor().ImageHandle != ImageHandleNull)
        {
            OIVCommands::UnloadImage(GetDescriptor().ImageHandle);
            GetDescriptorMutable().ImageHandle = ImageHandleNull;
        }
    }
    void OIVBaseImage::QueryImageInfo()
    {
        OIV_CMD_QueryImageInfo_Response textImageInfo;
        OIV_CMD_QueryImageInfo_Request loadRequest;
        loadRequest.handle = GetDescriptor().ImageHandle;
        ResultCode result = OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_QueryImageInfo, &loadRequest, &textImageInfo);
        GetDescriptorMutable().Bpp = textImageInfo.bitsPerPixel;
        GetDescriptorMutable().Height = textImageInfo.height;
        GetDescriptorMutable().Width = textImageInfo.width;
    }
    void OIVBaseImage::ResetActiveImageProperties()
    {
        fImagePropertiesCached = {};
    }
    void OIVBaseImage::SetImageHandle(ImageHandle imageHandle)
    {
        fDescriptor.ImageHandle = imageHandle;
        fImageProperties.imageHandle = imageHandle;
    }
}