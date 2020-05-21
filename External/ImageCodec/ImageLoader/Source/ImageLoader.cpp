#include "ImageLoader.h"
#include "EmbeddedPluginInstaller.h"
#include <LLUtils/StringUtility.h>
#include <LLUtils/StopWatch.h>

namespace IMCodec
{

    ImageLoader::ImageLoader()
    {
        EmbeddedPluginInstaller::InstallPlugins(this);
    }

    IImagePlugin* ImageLoader::GetFirstPlugin(const std::wstring& hint) const
    {
        using namespace std;
        IImagePlugin* result = nullptr;
        auto it = fMapPlugins.find(hint);
        if (it != fMapPlugins.end())
        {
            const ListPlugin& pluginsList = it->second;
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

    bool ImageLoader::TryLoad(IImagePlugin* plugin, uint8_t* buffer, std::size_t size, VecImageSharedPtr& out_images) const
    {
        VecImageDescriptors descriptors(1);
        LLUtils::StopWatch stopWatch(true);
        
        bool loaded = false;

        if (plugin->GetPluginProperties().hasMultipleImages)
        {
            
            loaded = plugin->LoadImages(buffer, size, descriptors);
        }
        else 
        {
            loaded = plugin->LoadImage(buffer, size, descriptors[0]);
            descriptors[0].fMetaData.LoadTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);
        }

        if (loaded == true)
        {
            for (const ImageDescriptor& imageDescriptor : descriptors)
                if ((imageDescriptor.IsInitialized() == true)) // verify image descriptor is properly initialized
                    out_images.push_back(std::make_shared<Image>(imageDescriptor));
        }

        return loaded;
    }

    bool ImageLoader::Load(uint8_t* buffer, std::size_t size, char* extension, bool onlyRegisteredExtension, VecImageSharedPtr& out_images) const
    {
       VecImageDescriptors imageDescriptors;

       bool loaded = false;

        
        IImagePlugin* choosenPlugin = GetFirstPlugin(LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ToWString(extension)));
        if (choosenPlugin != nullptr)
            loaded = TryLoad(choosenPlugin, buffer, size, out_images);

        
        // If image not loaded and allow to load using unregistred file extensions, iterate over all image plugins.
        if (loaded == false && onlyRegisteredExtension == false)
            for (auto plugin : fListPlugins)
                // In case we try to a choosen plugin and it failed. don't try again.
                if (plugin != choosenPlugin)
                    loaded = TryLoad(plugin, buffer, size, out_images);

        return loaded;
    }

    std::wstring ImageLoader::GetKnownFileTypes() const
    {
        std::wstringstream ss;
        for (const auto& pair : fMapPlugins)
            ss << pair.first << ";";

        return ss.str();

    }
}
