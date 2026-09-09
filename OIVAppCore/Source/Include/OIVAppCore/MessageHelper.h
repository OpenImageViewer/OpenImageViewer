#pragma once

#include <LLUtils/StringDefs.h>
#include <OIVImage/OIVBaseImage.h>
#include <string_view>

namespace IMCodec
{
    class ImageCodec;
}

namespace OIV
{
    class IRenderer;

    // UTF-8 metadata and renderer are borrowed for the duration of CreateSystemInfoMessage.
    struct SystemInfoContext
    {
        std::string_view appName;
        std::string_view appVersion;
        std::string_view gitHash;
        std::string_view buildType;
        const IRenderer* renderer = nullptr;
    };

    class MessageHelper
    {
      public:

        static LLUtils::native_string_type CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage,
                                                                  const OIVBaseImageSharedPtr& rasterized,
                                                                  IMCodec::ImageCodec& imageCodec);
        static LLUtils::native_string_type CreateKeyBindingsMessage();
        // Collect system and renderer details on the renderer's owning thread, then format the overlay.
        static LLUtils::native_string_type CreateSystemInfoMessage(const SystemInfoContext& context);
        static LLUtils::native_string_type GetFileTime(const LLUtils::native_string_type& filePath);
    };
}  // namespace OIV
