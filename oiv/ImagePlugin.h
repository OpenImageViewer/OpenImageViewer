#pragma once
#include <string>
#include "ImageProperties.h"
namespace OIV
{
    struct PluginProperties
    {
        std::string pluginDescription;
        std::string supportedExtentions;
    };
    class ImagePlugin
    {
    public:
        virtual bool LoadImage(void* buffer, size_t size, ImageProperies& out_properties) = 0;
        virtual PluginProperties& GetPluginProperties() = 0;
    };
}
