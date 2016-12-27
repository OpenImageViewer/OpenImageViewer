#pragma once
#include <Interfaces/IImagePlugin.h>
#include "libpsd-0.9/include/libpsd.h"

namespace OIV
{
    class CodecPSD : public IImagePlugin
    {

    public:
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties = { "psdlib codec","psd" };
            return pluginProperties;
        }

        bool LoadImage(void* buffer, size_t size, ImageProperies& out_properties) override
        {
            bool success = false;
            psd_context * context = nullptr;
            psd_status status;
            status = psd_image_load_merged(&context, static_cast<psd_char*>(buffer), size);

            if (status == psd_status_done)
            {
                out_properties.NumSubImages = 0;
                out_properties.ImageBuffer = reinterpret_cast<uint8_t*>(context->merged_image_data);
                out_properties.Width = context->width;
                out_properties.Height = context->height;
                out_properties.Type = IT_BYTE_BGRA;
                out_properties.BitsPerTexel = 32;
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
