#include "OIVBaseImage.h"
#include "../OIVCommands.h"
#include "OIVHandleImage.h"

namespace OIV
{
    OIVBaseImage::OIVBaseImage(bool freeAtDestruction)
    {
        fFreeAtDestruction = freeAtDestruction;
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
        if (fFreeAtDestruction == true)
            FreeImage();
    }

    ResultCode OIVBaseImage::DoUpdate()
    {
        ResultCode result = ResultCode::RC_UknownError;
        if (memcmp(&fImagePropertiesCached, &fImageProperties, sizeof(fImageProperties)) != 0)
        {
            CmdNull NullCommand;
            result = OIVCommands::ExecuteCommand(OIV_CMD_ImageProperties, &fImageProperties, &NullCommand);
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

    void OIVBaseImage::FetchSubImages()
    {
        const uint8_t numSubImages = GetDescriptor().NumSubImages;
        if (numSubImages > 0)
        {
            fSubImages.resize(numSubImages);

            std::unique_ptr<ImageHandle[]> imageArray = std::make_unique<ImageHandle[]>(numSubImages);

            OIV_CMD_GetSubImages_Request request = {};
            OIV_CMD_GetSubImages_Response response;
            request.handle = GetDescriptor().ImageHandle;
            request.arraySize = numSubImages;
            request.childrenArray = imageArray.get();

            if (OIVCommands::ExecuteCommand(OIV_CMD_GetSubImages, &request, &response) == RC_Success)
            {
                fSubImages.resize(response.copiedElements);

                //assert(response.copiedElements == numSubImages)

                for (int i = 0 ; i < fSubImages.size(); i++)
                {
                    fSubImages[i] = std::make_shared<OIVHandleImage>(imageArray[i], false);
                }

            }
        }
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
        GetDescriptorMutable().NumSubImages = textImageInfo.NumSubImages;

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
