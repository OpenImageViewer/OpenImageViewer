#pragma once

#include <IImagePlugin.h>
#include <turbojpeg.h>

namespace IMCodec
{
    class CodecJpeg : public IImagePlugin
    {
    private:
       


    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "Jpeg plugin codec","jpg;jpeg" };
            return pluginProperties;
        }

        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            static tjhandle ftjHandle = tjInitDecompress();
            bool success = false;
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 4;
            int subsamp;
            unsigned long jpegSize = static_cast<unsigned long>(size);
            if (tjDecompressHeader2(ftjHandle,const_cast<unsigned char*>( buffer), jpegSize, &width, &height, &subsamp) != -1)
            {
                size_t imageDataSize = width * height * bytesPerPixel;
                out_properties.fData.Allocate(imageDataSize);

                if (tjDecompress2(ftjHandle, const_cast< uint8_t*>(reinterpret_cast<const uint8_t*>( buffer)), jpegSize, reinterpret_cast<unsigned char*>( out_properties.fData.data()), width, width * bytesPerPixel, height, TJPF_RGBA, 0) != -1)
                {
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    out_properties.fProperties.Width = width;
                    out_properties.fProperties.Height = height;
                    out_properties.fProperties.RowPitchInBytes = bytesPerPixel * width;
                    out_properties.fProperties.NumSubImages = 0;

                    success = true;
                }
            }
            return success;
        }
    };
}
