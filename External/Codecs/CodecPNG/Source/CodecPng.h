#pragma once

#include <IImagePlugin.h>
#include <png.h>
#include <LLUtils/BitFlags.h>

namespace IMCodec
{
    class CodecPNG : public IImagePlugin
    {
    private:
        enum class PngFormatFlags : png_uint_32
        {
              None = 0 << 0
            , Alpha = 1 << 0
            , Color = 1 << 1
            , Linear = 1 << 2
            , ColorMap = 1 << 3
            , Reserved5 = 1 << 4
            , Reserved6 = 1 << 5
            , Reserved7 = 1 << 6
            , Reserved8 = 1 << 7
            , Reserved9 = 1 << 8

        };

		LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS_IN_CLASS(PngFormatFlags)

            
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

                int numChannles = 0;
                int sizeofChannel = 0;
                LLUtils::Buffer colorMap;

                switch (image.format)
                {
                case PNG_FORMAT_GRAY:
                      out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_X8;
                      break;
                case PNG_FORMAT_RGBA:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    break;
                case PNG_FORMAT_RGB:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8;
                    break;
                case PNG_FORMAT_FLAG_ALPHA | PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP:
                    numChannles = 4;
                    sizeofChannel = 1;
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    out_properties.fProperties.RowPitchInBytes = image.width * numChannles;
                    break;
                case  PNG_FORMAT_FLAG_COLOR | PNG_FORMAT_FLAG_COLORMAP:
                    numChannles = 3;
                    sizeofChannel = 1;
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8;
                    out_properties.fProperties.RowPitchInBytes = image.width * numChannles;
                    break;
                default:
                    LL_EXCEPTION_NOT_IMPLEMENT("Png image format not supported");

                }

                 LLUtils::BitFlags<PngFormatFlags> formatFlags( static_cast<PngFormatFlags>(image.format));


                 if (formatFlags.test(PngFormatFlags::ColorMap))
                     colorMap.Allocate(numChannles * sizeofChannel * image.colormap_entries);

                    
                out_properties.fProperties.NumSubImages = 0;

                //read buffer
                out_properties.fData.Allocate(PNG_IMAGE_SIZE(image));

                if (out_properties.fData.GetBuffer() != nullptr &&
                    png_image_finish_read(&image, nullptr/*background*/, out_properties.fData.GetBuffer(),
                        PNG_IMAGE_ROW_STRIDE(image), colorMap.GetBuffer()) != 0)
                {

                    if (colorMap != nullptr)
                    {
                        //Resolve color map to a bitmap
                        size_t pixelSize = numChannles * sizeofChannel;
                        LLUtils::Buffer unmappedBuuffer(numChannles * image.width * image.height);
                        
                        for (size_t pixelIndex = 0; pixelIndex < image.width * image.height; pixelIndex++)
                        {
                            size_t colorIndex = ((uint8_t*)(out_properties.fData.GetBuffer()))[pixelIndex];

                            size_t sourcePos = colorIndex * pixelSize;
                            size_t destPos = pixelIndex * pixelSize;
                            
                            memcpy( reinterpret_cast<uint8_t*>(unmappedBuuffer.GetBuffer()) + destPos
                                , reinterpret_cast<uint8_t*>(colorMap.GetBuffer()) + sourcePos, pixelSize);
                        }
                        out_properties.fData = std::move(unmappedBuuffer);
                    }

                    success = true;
                }
            }
            return success;
        }
    };
}
