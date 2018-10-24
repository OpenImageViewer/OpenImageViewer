#include "EmbeddedPluginInstaller.h"
#include "ImageLoader.h"
#include "BuildConfig.h"

//TODO: Take configuration out to a new file
#if IMCODEC_BUILD_CODEC_PSD == 1
    #include <Codecs/CodecPSD/Include/CodecPSDFactory.h>
#endif
#if IMCODEC_BUILD_CODEC_JPG == 1
    #include <Codecs/CodecJpg/Include/CodecJPGFactory.h>
#endif
#if IMCODEC_BUILD_CODEC_PNG == 1
    #include "Codecs/CodecPNG/Include/CodecPNGFactory.h"
#endif
#if IMCODEC_BUILD_CODEC_DDS == 1
#include "Codecs/CodecDDS/Include/CodecDDSFactory.h"
#endif
#if IMCODEC_BUILD_CODEC_FREE_IMAGE == 1
    #include "Codecs/CodecFreeImage/Include/CodecFreeImageFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_TIFF == 1
#include "Codecs/CodecTiff/Include/CodecTiffFactory.h"
#endif

#if IMCODEC_BUILD_CODEC_GIF == 1
#include "Codecs/CodecGif/Include/CodecGifFactory.h"
#endif






namespace IMCodec
{
    bool EmbeddedPluginInstaller::InstallPlugins(ImageLoader* imageLoader)
    {
        // install codec by priority, first installed is with the higher priority.
#if IMCODEC_BUILD_CODEC_PSD == 1
        imageLoader->InstallPlugin(CodecPSDFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_JPG == 1
        imageLoader->InstallPlugin(CodecJPGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_PNG == 1
        imageLoader->InstallPlugin(CodecPNGFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_DDS == 1
        imageLoader->InstallPlugin(CodecDDSFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_TIFF == 1
        imageLoader->InstallPlugin(CodecTiffFactory::Create());
#endif
#if IMCODEC_BUILD_CODEC_GIF == 1
        imageLoader->InstallPlugin(CodecGifFactory::Create());
#endif

// keep freeimage the last priority codec as it's inferior
#if IMCODEC_BUILD_CODEC_FREE_IMAGE == 1
        imageLoader->InstallPlugin(CodecFreeImageFactory::Create());
#endif
        return true;
    }
}
