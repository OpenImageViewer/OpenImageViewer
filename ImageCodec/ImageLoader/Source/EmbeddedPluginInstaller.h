#pragma once
namespace IMCodec
{
    class ImageLoader;
    class EmbeddedPluginInstaller
    {
    public:
        static bool InstallPlugins(ImageLoader* imageLoader);
    };
}