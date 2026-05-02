#pragma once

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
            std::wstring text;
        };

        struct ImageInfoRow
        {
            std::string key;
            std::vector<ImageInfoValue> values;
        };

        using ImageInfoRows = std::vector<ImageInfoRow>;

        static ImageInfoRows Build(const OIVBaseImageSharedPtr& image,
                                   const OIVBaseImageSharedPtr& rasterized,
                                   IMCodec::IImageCodec& imageCodec);
        static std::wstring FormatFileTime(const std::filesystem::path& filePath);
        static std::wstring FormatImageSource(ImageSource source);
        static std::wstring FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);
    };
}  // namespace OIV
