#include "ImageLoader.h"
#include "EmbeddedPluginInstaller.h"
#include <StringUtility.h>
#include <StopWatch.h>

namespace IMCodec
{

    ImageLoader::ImageLoader()
    {
        EmbeddedPluginInstaller::InstallPlugins(this);
    }

    IImagePlugin* ImageLoader::GetFirstPlugin(const std::wstring& hint)
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

    void ImageLoader::InstallPlugin(IImagePlugin* plugin)
    {
        using namespace LLUtils;
        fListPlugins.push_back(plugin);
        ListString tokens = StringUtility::split(StringUtility::ToLower(
            StringUtility::ToWString(plugin->GetPluginProperties().supportedExtentions)), ';');

        for (auto token : tokens)
        {
            auto it = fMapPlugins.find(token);
            if (it == fMapPlugins.end())
                it = fMapPlugins.insert(std::make_pair(token, ListPlugin())).first;

            ListPlugin& pluginsList = it->second;
            pluginsList.push_back(plugin);
        }
    }

    Image* ImageLoader::Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension)
    {
        using namespace std;

        Image* loadedImage = nullptr;

        IImagePlugin* choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ToWString(extension)));

        double loadTime;
        ImageProperies props;
        if (choosenPlugin != nullptr)
        {
            auto start = std::chrono::high_resolution_clock::now();
            if (choosenPlugin->LoadImage(buffer, size, props))
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
                LLUtils::StopWatch stopWatch;
                stopWatch.Start();
                if (plugin->LoadImage(buffer, size, props))
                {
                    stopWatch.Stop();

                    if ((props.IsInitialized() == false))
                        throw std::runtime_error("Image properties are not completely initialized");

                    loadTime = stopWatch.GetElapsedTime(LLUtils::StopWatch::TimeUnit::Milliseconds);
                    loadedImage = new Image(props, loadTime);
                }
            }
        }

        return loadedImage;
    }
}
