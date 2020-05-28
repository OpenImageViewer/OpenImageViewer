#pragma once
#include <IImagePlugin.h>
#include <webp/decode.h>

namespace IMCodec
{
    class CodecWebP : public IImagePlugin
    {

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "webP codec","webp" };
            return pluginProperties;
        }

        bool LoadImage(const uint8_t* buffer, size_t size, ImageDescriptor& out_properties) override
        {
            int width;
            int height;
            
            
            if (WebPGetInfo(reinterpret_cast<const std::uint8_t*>(buffer), size, &width, &height) != 0)
            {
                const size_t decodedBufferSize = width * height * 4;
                LLUtils::Buffer decodedBuffer(decodedBufferSize);
                
                if (WebPDecodeBGRAInto(reinterpret_cast<const std::uint8_t*>(buffer), size, reinterpret_cast<uint8_t*>(decodedBuffer.data()),
                    decodedBufferSize, width * 4) != nullptr)
                {
                    out_properties.fProperties.Height = height;
                    out_properties.fProperties.Width = width;
                    out_properties.fProperties.RowPitchInBytes = width * 4;
                    out_properties.fProperties.NumSubImages = 0;
                    out_properties.fProperties.TexelFormatStorage = TexelFormat::I_B8_G8_R8_A8;
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                    out_properties.fData = std::move(decodedBuffer);

                    const size_t imageSize = out_properties.fProperties.Height * out_properties.fProperties.RowPitchInBytes;
                    /*out_properties.fData.Allocate(imageSize);
                    out_properties.fData.Write(reinterpret_cast<std::byte*>(decoded), 0, imageSize);*/
                    return true;
                }

            }
            
          /*  uint8_t* decoded = WebPDecodeBGRA();
            if (decoded != nullptr)
            {
                out_properties.fProperties.Height = height;
                out_properties.fProperties.Width = width;
                out_properties.fProperties.RowPitchInBytes = height * 4;
                out_properties.fProperties.NumSubImages = 0;
                out_properties.fProperties.TexelFormatStorage = TexelFormat::I_B8_G8_R8_A8;
                out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;

                const size_t imageSize = out_properties.fProperties.Height * out_properties.fProperties.RowPitchInBytes;
                out_properties.fData.Allocate(imageSize);
                out_properties.fData.Write(reinterpret_cast<std::byte*>(decoded), 0, imageSize);
                WebPFree(decoded);
                return true;
            }*/

            return false;

            
            
        }
    };
 
}
