#pragma once
#include <string>
#include <vector>
#include <memory>
#include <LLUtils/StopWatch.h>
#include <defs.h>

namespace OIV
{
    enum class ImageSource
    {
          None
        , File
        , Clipboard
        , InternalText
        , GeneratedByLib
    };

    struct ImageDescriptor
    {
        OIV_TexelFormat texelFormat = TF_UNKNOWN;
        ImageHandle ImageHandle = ImageHandleNull;
        ImageSource Source = ImageSource::None;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint8_t Bpp = 0;
        uint8_t NumSubImages = 0;
        LLUtils::StopWatch::time_type_real DisplayTime = 0.0;
        double LoadTime = 0.0;

    };
  
    class OIVBaseImage
    {
    public:
        OIVBaseImage(bool freeAtDestruction = true);
        //Base image updates image properties
        ResultCode Update();
        virtual ~OIVBaseImage();

    public: //const methods
        const ImageDescriptor& GetDescriptor() {return fDescriptor; }
        std::wstring GetDescription() const;
        OIV_CMD_ImageProperties_Request& GetImageProperties()  { return fImageProperties; }
        void FetchSubImages();
        std::vector<std::shared_ptr<OIVBaseImage>>& GetSubImages()
        {
            return fSubImages;
        }
    protected:
        OIV_CMD_ImageProperties_Request& GetImagePropertiesCurrent();
        void FreeImage();
        void QueryImageInfo();
        void ResetActiveImageProperties();
        void SetImageHandle(ImageHandle imageHandle);
        virtual ResultCode DoUpdate();
        ImageDescriptor& GetDescriptorMutable() { return fDescriptor; }
    private:


    private: //member fields
        OIV_CMD_ImageProperties_Request fImagePropertiesCached = {};
        OIV_CMD_ImageProperties_Request fImageProperties = {};
        ImageDescriptor fDescriptor;
        std::vector<std::shared_ptr<OIVBaseImage>> fSubImages;
        bool fFreeAtDestruction = true;
    };

    using OIVBaseImageUniquePtr = std::shared_ptr<OIVBaseImage>;
    using OIVBaseImageSharedPtr = std::shared_ptr<OIVBaseImage>;
}