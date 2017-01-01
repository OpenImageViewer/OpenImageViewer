#pragma once
#include "../Configuration.h"

#if OIV_BUILD_PLUGIN_JPEG == 1
#include "../Interfaces/IImagePlugin.h"
#include <turbojpeg.h>

namespace OIV
{
    class PluginJpeg : public IImagePlugin
    {
    private:
        static tjhandle ftjHandle;

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "Jpeg plugin codec","jpg;jpeg" };
            return pluginProperties;
        }

        virtual bool LoadImage(void* buffer, std::size_t size, ImageProperies& out_properties) override
        {
            bool success = false;
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 4;
            int subsamp;
            unsigned long jpegSize = static_cast<unsigned long>(size);
            if (tjDecompressHeader2(ftjHandle,static_cast<unsigned char*>( buffer), jpegSize, &width, &height, &subsamp) != -1)
            {
                unsigned char* bufferDecompressed = new unsigned char[width * height * bytesPerPixel];

                if (tjDecompress2(ftjHandle, static_cast<unsigned char*>(buffer), jpegSize, bufferDecompressed, width, width * bytesPerPixel, height, TJPF_RGBA, 0) != -1)
                {
                    out_properties.ImageBuffer = static_cast<uint8_t*>(bufferDecompressed);
                    out_properties.BitsPerTexel = bytesPerPixel * 8;
                    out_properties.Type = IT_BYTE_RGBA;
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
    tjhandle PluginJpeg::ftjHandle = tjInitDecompress();
}
#endif