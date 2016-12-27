#pragma once
#include "Interfaces/IImagePlugin.h"
#include <vector>
#include <map>
#include <chrono>
#include <Utility.h>
#include <StringUtility.h>


namespace OIV
{
    class ImageLoader
    {
        typedef std::vector<IImagePlugin*> ListPlugin;
        typedef std::map<std::string, ListPlugin> MapStringListPlugin;
        MapStringListPlugin fMapPlugins;
        ListPlugin fListPlugins;

        IImagePlugin* GetFirstlugin(const std::string& hint)
        {
            using namespace std;
            IImagePlugin* result = nullptr;
            auto it = fMapPlugins.find(hint);
            if (it != fMapPlugins.end())
            {
                ListPlugin& pluginsList = it->second;
                result = *pluginsList.begin();
            }

            return result;
        }
    public:
        void InstallPlugin(IImagePlugin* plugin)
        {
            fListPlugins.push_back(plugin);
            ListString tokens = StringUtility::split(StringUtility::ToLower(plugin->GetPluginProperties().supportedExtentions) , ';');

            for (auto token : tokens)
            {
                auto it = fMapPlugins.find(token);
                if (it == fMapPlugins.end())
                    it = fMapPlugins.insert(make_pair(token, ListPlugin())).first;

                ListPlugin& pluginsList = it->second;
                pluginsList.push_back(plugin);
            }
        }
    
        Image* LoadImage(void* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension = true)
        {
            using namespace std;

            Image* loadedImage = nullptr;
            
            IImagePlugin* choosenPlugin = GetFirstlugin(StringUtility::ToLower(extension));

            double loadTime;
            ImageProperies props;
            if (choosenPlugin != nullptr)
            {
                
                auto start = std::chrono::high_resolution_clock::now();
                if (choosenPlugin->LoadImage(buffer,size, props))
                {
                    auto end = std::chrono::high_resolution_clock::now();
                    if ((props.IsInitialized() == true))
                    {
                        loadTime = (end - start).count() / static_cast<long double>(1000.0 * 1000.0);
                        loadedImage = new Image(props, loadTime);
                    }
                }
            }
            else if (onlyRegisteredExtension == false)
            {
                // Iterate all plugins till load succeeds.
                for (auto plugin : fListPlugins)
                {
                    auto start = std::chrono::high_resolution_clock::now();
                    if (plugin->LoadImage(buffer, size, props))
                    {
                        auto end = std::chrono::high_resolution_clock::now();
                        if ((props.IsInitialized() == false))
                            throw std::exception("Image properties are not completely initialized");
                        loadTime = (end - start).count() / static_cast<long double>(1000.0 * 1000.0);
                        loadedImage = new Image(props, loadTime);
                    }
                }
            }

            return loadedImage;
        }
    };
}
