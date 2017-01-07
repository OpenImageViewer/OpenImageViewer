#pragma once
#include "IImagePlugin.h"
#include <unordered_map>
#include <chrono>
#include <vector>


namespace IMCodec
{
    class ImageLoader
    {
        typedef std::vector<IImagePlugin*> ListPlugin;
        typedef std::unordered_map<std::wstring, ListPlugin> MapStringListPlugin;
        MapStringListPlugin fMapPlugins;
        ListPlugin fListPlugins;

        IImagePlugin* GetFirstPlugin(const std::wstring& hint);
    public:

        ImageLoader();
        void InstallPlugin(IImagePlugin* plugin);
        Image* Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension = true);
    };

   
}
