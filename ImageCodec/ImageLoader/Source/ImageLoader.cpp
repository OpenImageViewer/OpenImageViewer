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
        ListWString tokens = StringUtility::split(StringUtility::ToLower(
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

    Image* ImageLoader::TryLoad(IImagePlugin* plugin, uint8_t* buffer, std::size_t size)
    {
        ImageDescriptor props;
        LLUtils::StopWatch stopWatch(true);
        Image* loadedImage = nullptr;
        if (plugin->LoadImage(buffer, size, props))
        {
            
            props.fMetaData.LoadTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);
            if ((props.IsInitialized() == true))
                loadedImage = new Image(props);

        }

        return loadedImage;
    }

    Image* ImageLoader::Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension)
    {
        Image* loadedImage = nullptr;
        
        IImagePlugin* choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ToWString(extension)));
        if (choosenPlugin != nullptr)
            loadedImage = TryLoad(choosenPlugin, buffer, size);
        
        // If image not loaded and allow to load using unregistred file extensions, iterate over all image plugins.
        if (loadedImage == nullptr && onlyRegisteredExtension == false)
            for (auto plugin : fListPlugins)
                // In case we try to a choosen plugin and it failed. don't try again.
                if (plugin != choosenPlugin)
                    loadedImage = TryLoad(plugin, buffer, size);

        return loadedImage;
    }
}
