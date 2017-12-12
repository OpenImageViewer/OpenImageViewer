#pragma once
#include <string>
#include "Image.h"
namespace IMCodec
{
    struct PluginProperties
    {
        std::string pluginDescription;
        std::string supportedExtentions;
    };
    class IImagePlugin
    {
    public:
        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) = 0;
        virtual bool SaveImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties)
        {
            throw std::runtime_error("not implemented");
        }
        virtual PluginProperties& GetPluginProperties() = 0;
    };
}
