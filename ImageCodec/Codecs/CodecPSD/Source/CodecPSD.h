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

        bool LoadImage(const uint8_t* buffer, size_t size, ImageProperies& out_properties) override
        {
            bool success = false;
            psd_context * context = nullptr;
            psd_status status;
            status = psd_image_load_merged_from_memory(&context, 
                reinterpret_cast<psd_char*>(const_cast<uint8_t*>(buffer)), size);

            if (status == psd_status_done)
            {
                out_properties.NumSubImages = 0;
                out_properties.ImageBuffer = reinterpret_cast<uint8_t*>(context->merged_image_data);
                out_properties.Width = context->width;
                out_properties.Height = context->height;
                out_properties.TexelFormatDecompressed = TF_I_B8_G8_R8_A8;
                out_properties.RowPitchInBytes = 4 * context->width;
                
                // 'merged_image_data' ownership has been delegated to image properties - do not free memory.
                context->merged_image_data = nullptr;
                psd_image_free(context);
                success = true;
            }

            return success;
        }
    };
 
}
