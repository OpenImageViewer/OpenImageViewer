#pragma once
#include "ImagePlugin.h"
#include <vector>
#include <chrono>
namespace OIV
{
    class ImageLoader
    {
        typedef std::vector<ImagePlugin*> PluginsList;
        PluginsList fPlugins;

    public:
        void InstallPlugin(ImagePlugin* plugin)
        {
            fPlugins.push_back(plugin);
        }
    
        Image* LoadImage(std::string fileName)
        {
            Image* loadedImage = NULL;
            using namespace std;
            int pos = fileName.find_last_of(".");
            string extension;
            ImagePlugin* choosenPlugin = nullptr;
            if (pos != string::npos)
                extension = fileName.substr(pos + 1, fileName.length() - pos - 1);
            if (extension.empty() == false)
            {

                for (auto plugin : fPlugins) 
                {
                    if (string(plugin->GetHint()).find(extension) != string::npos)
                    {
                        choosenPlugin = plugin;
                        break;
                    }
                    
                }
             
            }

            if (choosenPlugin != nullptr)
            {
                double loadTime;
                ImageProperies props;
                auto start = std::chrono::high_resolution_clock::now();
                if (choosenPlugin->LoadImage(fileName, props))
                {
                    auto end = std::chrono::high_resolution_clock::now();
                    if ((props.IsInitialized() == false))
                        throw std::exception("Image properties are not completely initialized");
                    loadTime = (end - start).count() / static_cast<long double>(1000.0 * 1000.0);
                    loadedImage = new Image(props, loadTime);
                }
            }

            return loadedImage;
        }
    };
}
