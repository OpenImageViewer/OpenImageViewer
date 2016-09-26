#pragma once
#include "ImagePlugin.h"
#include <png.h>
namespace OIV
{
    class PluginPng : public ImagePlugin
    {
    public:

        virtual char* GetHint() override
        {
            return "png";
        }

        //Base abstract methods
        virtual bool LoadImage(std::string filePath, ImageProperies& out_properties) override
        {
            using namespace std;
            const char* file_name = filePath.c_str();
            bool success = false;

            png_image image; /* The control structure used by libpng */

            /* Initialize the 'png_image' structure. */
            memset(&image, 0, (sizeof image));
            image.version = PNG_IMAGE_VERSION;
            if (png_image_begin_read_from_file(&image, file_name) != 0)
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
                void* buffer = static_cast<BYTE*>(malloc(PNG_IMAGE_SIZE(image)));

                if (buffer != nullptr &&
                    png_image_finish_read(&image, NULL/*background*/, buffer,
                        PNG_IMAGE_ROW_STRIDE(image), NULL/*colormap*/) != 0)
                {
                    success = true;
                    out_properties.ImageBuffer = buffer;
                }
            }
            return success;
        }
    };
}