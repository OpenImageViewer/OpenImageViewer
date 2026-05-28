#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVImage/OIVBaseImage.h>
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
            LLUtils::native_string_type text;
        };

        struct ImageInfoRow
        {
            std::string key;
            std::vector<ImageInfoValue> values;
        };

        using ImageInfoRows = std::vector<ImageInfoRow>;

        static ImageInfoRows Build(const OIVBaseImageSharedPtr& image, const OIVBaseImageSharedPtr& rasterized,
                                   IMCodec::IImageCodec& imageCodec);
        static LLUtils::native_string_type FormatFileTime(const std::filesystem::path& filePath);
        static LLUtils::native_string_type FormatImageSource(ImageSource source);
        static LLUtils::native_string_type FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);
    };
}  // namespace OIV
