#pragma once
#include <string>
#include <image/Image.h>
namespace OIV
{
    struct PluginProperties
    {
        std::string pluginDescription;
        std::string supportedExtentions;
    };
    class IImagePlugin
    {
    public:
        virtual bool LoadImage(void* buffer, std::size_t size, ImageProperies& out_properties) = 0;
        virtual PluginProperties& GetPluginProperties() = 0;
    };
}
