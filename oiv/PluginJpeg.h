#pragma once
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

        virtual bool LoadImage(std::string filePath, ImageProperies& out_properties) override
        {
            bool success = false;
            
            std::ifstream file(filePath, std::ifstream::ate | std::ifstream::binary);

            if (file.is_open())
            {
                size_t fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                unsigned char* byteArray = new unsigned char[fileSize];
                file.read((char*)byteArray, fileSize);

                int width = 0;
                int height = 0;

                int bytesPerPixel = 3;
                int subsamp;
                if (tjDecompressHeader2(ftjHandle, byteArray, fileSize, &width, &height, &subsamp) != -1)
                {
                    unsigned char* buffer = new unsigned char[width * height * bytesPerPixel];

                    if (tjDecompress2(ftjHandle, byteArray, fileSize, buffer, width, width * bytesPerPixel, height, TJPF_RGB, 0) != -1)
                    {
                        out_properties.ImageBuffer = (void*)buffer;
                        out_properties.BitsPerTexel = bytesPerPixel * 8;
                        out_properties.Type = IT_BYTE_BGR;
                        out_properties.Width = width;
                        out_properties.Height = height;
                        out_properties.RowPitchInBytes = bytesPerPixel * width;
                        out_properties.NumSubImages = 0;

                        success = true;
                    }
                }
                delete[]byteArray;
            }
            
            return success;

        }
    };
    tjhandle PluginJpeg::ftjHandle = tjInitDecompress();
}
