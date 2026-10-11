#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/ViewerImage.h>
#include <TexelFormat.h>

#include <filesystem>
#include <string>
#include <vector>

namespace IMCodec
{
    class IImageCodec;
}

namespace OIV
{
    class ImageInfoPresentationPolicy
    {
      public:

        struct ImageInfoValue
        {
            // Formatted native text for one information value.
            LLUtils::native_string_type text;
        };

        struct ImageInfoRow
        {
            // Stable information-row label used by formatting policy.
            std::string key;
            // Ordered formatted values in this row.
            std::vector<ImageInfoValue> values;
        };

        using ImageInfoRows = std::vector<ImageInfoRow>;

        static ImageInfoRows Build(const ViewerImage* image, const IMCodec::ImageSharedPtr& rasterized,
                                   double displayTime, int64_t uniqueValues, IMCodec::IImageCodec& imageCodec);
        static LLUtils::native_string_type FormatFileTime(const std::filesystem::path& filePath);
        static LLUtils::native_string_type FormatImageSource(ImageSource source);
        static LLUtils::native_string_type FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);
    };
}  // namespace OIV
