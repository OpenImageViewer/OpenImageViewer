#pragma once

#include <LLUtils/StringDefs.h>
#include <OIVAppCore/ViewerImage.h>
#include <string_view>
#include <optional>
#include <RendererCore/RendererOptions.h>

namespace IMCodec
{
    class ImageCodec;
}

namespace OIV
{
    class Renderer;

    // UTF-8 metadata and renderer are borrowed for the duration of CreateSystemInfoMessage.
    struct SystemInfoContext
    {
        // Borrowed UTF-8 application name.
        std::string_view appName;
        // Borrowed UTF-8 formatted application version.
        std::string_view appVersion;
        // Borrowed UTF-8 source revision.
        std::string_view gitHash;
        // Borrowed UTF-8 build configuration name.
        std::string_view buildType;
        // Optional borrowed common facade, queried only on its owner thread.
        const Renderer* renderer = nullptr;
        // Actual factory selection, absent when the caller has no backend identity.
        std::optional<RendererType> rendererType;
    };

    class MessageHelper
    {
      public:

        static LLUtils::native_string_type CreateImageInfoMessage(const ViewerImage* oivImage,
                                                                  const IMCodec::ImageSharedPtr& rasterized,
                                                                  double displayTime, int64_t uniqueValues,
                                                                  IMCodec::ImageCodec& imageCodec);
        static LLUtils::native_string_type CreateKeyBindingsMessage();
        // Collect system and renderer details on the renderer's owning thread, then format the overlay.
        static LLUtils::native_string_type CreateSystemInfoMessage(const SystemInfoContext& context);
        static LLUtils::native_string_type GetFileTime(const LLUtils::native_string_type& filePath);
    };
}  // namespace OIV
