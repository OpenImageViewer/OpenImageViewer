#pragma once
#include <IImagePlugin.h>
#include <libpsd.h>

namespace IMCodec
{
    class CodecPSD : public IImagePlugin
    {

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "psdlib codec","psd" };
            return pluginProperties;
        }

        bool LoadImage(const uint8_t* buffer, size_t size, ImageDescriptor& out_properties) override
        {
            bool success = false;
            psd_context * context = nullptr;
            psd_status status;
            status = psd_image_load_merged_from_memory(&context, 
                reinterpret_cast<psd_char*>(const_cast<uint8_t*>(buffer)), size);

            if (status == psd_status_done)
            {
                const uint32_t numChannels = 4;
                size_t mergedImageSize = context->per_channel_length * numChannels;
                out_properties.fData.Allocate(mergedImageSize);
                out_properties.fData.Write(reinterpret_cast<std::byte*>(context->merged_image_data), 0, mergedImageSize);

                out_properties.fProperties.NumSubImages = 0;
                out_properties.fProperties.Width = context->width;
                out_properties.fProperties.Height = context->height;
                out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                out_properties.fProperties.RowPitchInBytes = numChannels * context->width;
                
                psd_image_free(context);
                success = true;
            }

            return success;
        }
    };
 
}
