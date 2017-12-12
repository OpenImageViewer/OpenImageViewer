#pragma once

#include <IImagePlugin.h>
#include <png.h>
namespace IMCodec
{
    class CodecPNG : public IImagePlugin
    {
    public:

        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "PNG plugin codec","png" };
            return pluginProperties;
        }

        //Base abstract methods
        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            using namespace std;
            
            bool success = false;

            png_image image; /* The control structure used by libpng */

            /* Initialize the 'png_image' structure. */
            memset(&image, 0, (sizeof image));
            image.version = PNG_IMAGE_VERSION;
            if (png_image_begin_read_from_memory(&image, buffer,size) != 0)
            {
                
                //Assign image properties
                out_properties.fProperties.RowPitchInBytes = PNG_IMAGE_ROW_STRIDE(image);
                out_properties.fProperties.Width = image.width;
                out_properties.fProperties.Height = image.height;

                if (image.format == PNG_FORMAT_RGBA)
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                if (image.format == PNG_FORMAT_RGB)
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8;
                out_properties.fProperties.NumSubImages = 0;

                //read buffer
                out_properties.fData.Allocate(PNG_IMAGE_SIZE(image));

                if (out_properties.fData.GetBuffer() != nullptr &&
                    png_image_finish_read(&image, nullptr/*background*/, out_properties.fData.GetBuffer(),
                        PNG_IMAGE_ROW_STRIDE(image), nullptr/*colormap*/) != 0)
                {
                    success = true;
                }
            }
            return success;
        }
    };
}
