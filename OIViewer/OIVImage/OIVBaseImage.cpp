#include "OIVBaseImage.h"
#include "../OIVCommands.h"
namespace OIV
{
    OIVBaseImage::OIVBaseImage()
    {
        fImageProperties.imageHandle = ImageHandleNull;;
        fImageProperties.position = 0;
        fImageProperties.filterType = OIV_Filter_type::FT_None;
        fImageProperties.imageRenderMode = OIV_Image_Render_mode::IRM_MainImage;
        fImageProperties.scale = 1.0;
        fImageProperties.opacity = 0.0;

    }

    ResultCode OIVBaseImage::Update()
    {
        return DoUpdate();
    }

    OIVBaseImage::~OIVBaseImage()
    {
        FreeImage();
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
            << fDescriptor.LoadTime << L" ms"
            //<< L"/" << fDescriptor.DisplayTime + fDescriptor.LoadTime << L" ms"
            ;

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
        GetDescriptorMutable().texelFormat = textImageInfo.texelFormat;
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