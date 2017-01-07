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
        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageProperies& out_properties) override
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
                out_properties.BitsPerTexel = PNG_IMAGE_PIXEL_SIZE(image.format) * 8;
                out_properties.RowPitchInBytes = PNG_IMAGE_ROW_STRIDE(image);
                out_properties.Width = image.width;
                out_properties.Height = image.height;

                if (image.format == PNG_FORMAT_RGBA)
                    out_properties.Type = IT_BYTE_RGBA;
                if (image.format == PNG_FORMAT_RGB)
                    out_properties.Type = IT_BYTE_BGR;
                out_properties.NumSubImages = 0;

                //read buffer
                void* buffer = malloc(PNG_IMAGE_SIZE(image));

                if (buffer != nullptr &&
                    png_image_finish_read(&image, nullptr/*background*/, buffer,
                        PNG_IMAGE_ROW_STRIDE(image), nullptr/*colormap*/) != 0)
                {
                    success = true;
                    out_properties.ImageBuffer = static_cast<uint8_t*>(buffer);
                }
            }
            return success;
        }
    };
}
