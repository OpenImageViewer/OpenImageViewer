#pragma once

#include <IImagePlugin.h>
#include <turbojpeg.h>

namespace IMCodec
{
    class CodecJpeg : public IImagePlugin
    {
    private:
        static tjhandle ftjHandle;

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "Jpeg plugin codec","jpg;jpeg" };
            return pluginProperties;
        }

        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageProperies& out_properties) override
        {
            bool success = false;
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 4;
            int subsamp;
            unsigned long jpegSize = static_cast<unsigned long>(size);
            if (tjDecompressHeader2(ftjHandle,const_cast<unsigned char*>( buffer), jpegSize, &width, &height, &subsamp) != -1)
            {
                unsigned char* bufferDecompressed = new unsigned char[width * height * bytesPerPixel];

                if (tjDecompress2(ftjHandle, const_cast<unsigned char*>(buffer), jpegSize, bufferDecompressed, width, width * bytesPerPixel, height, TJPF_RGBA, 0) != -1)
                {
                    out_properties.ImageBuffer = static_cast<uint8_t*>(bufferDecompressed);
                    out_properties.TexelFormatDecompressed = TF_I_R8_G8_B8_A8;
                    out_properties.Width = width;
                    out_properties.Height = height;
                    out_properties.RowPitchInBytes = bytesPerPixel * width;
                    out_properties.NumSubImages = 0;

                    success = true;
                }
            }
            return success;
        }
    };
    tjhandle CodecJpeg::ftjHandle = tjInitDecompress();
}
