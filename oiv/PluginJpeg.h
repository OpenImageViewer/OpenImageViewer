#pragma once
#include "ImagePlugin.h"
#include <turbojpeg.h>
#include <jpeglib.h>
#include "OgrePlugin.h"

namespace OIV
{
    class PluginJpeg : public ImagePlugin
    {
    private:
        static tjhandle ftjHandle;

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "Jpeg plugin codec","jpg;jpeg" };
            return pluginProperties;
        }

        virtual bool LoadImage(void* buffer, size_t size, ImageProperies& out_properties) override
        {
            bool success = false;
        
            int width = 0;
            int height = 0;

            int bytesPerPixel = 3;
            int subsamp;
            if (tjDecompressHeader2(ftjHandle,static_cast<unsigned char*>( buffer), size, &width, &height, &subsamp) != -1)
            {
                unsigned char* bufferDecompressed = new unsigned char[width * height * bytesPerPixel];

                if (tjDecompress2(ftjHandle, static_cast<unsigned char*>(buffer), size, bufferDecompressed, width, width * bytesPerPixel, height, TJPF_RGB, 0) != -1)
                {
                    out_properties.ImageBuffer = static_cast<void*>(bufferDecompressed);
                    out_properties.BitsPerTexel = bytesPerPixel * 8;
                    out_properties.Type = IT_BYTE_BGR;
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
