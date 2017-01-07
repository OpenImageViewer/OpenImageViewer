#include "EmbeddedPluginInstaller.h"
#include "ImageLoader.h"
#include "BuildConfig.h"

#if IMCODEC_BUILD_CODEC_PSD == 1
    #include <Codecs/CodecPSD/Include/CodecPSDFactory.h>
#endif
#if IMCODEC_BUILD_CODEC_JPG == 1
    #include <Codecs/CodecJpg/Include/CodecJPGFactory.h>
#endif
#if IMCODEC_BUILD_CODEC_PNG == 1
    #include "Codecs/CodecPNG/Include/CodecPNGFactory.h"
#endif
#if IMCODEC_BUILD_CODEC_FREE_IMAGE == 1
    #include "Codecs/CodecFreeImage/Include/CodecFreeImageFactory.h"
#endif




namespace IMCodec
{
    bool EmbeddedPluginInstaller::InstallPlugins(ImageLoader* imageLoader)
    {
#if IMCODEC_BUILD_CODEC_PSD == 1
        imageLoader->InstallPlugin(CodecPSDFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_JPG == 1
        imageLoader->InstallPlugin(CodecJPGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_PNG == 1
        imageLoader->InstallPlugin(CodecPNGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_FREE_IMAGE == 1
        imageLoader->InstallPlugin(CodecFreeImageFactory::Create());
#endif
        return true;
    }
}
