#pragma once

#include <OIVAppCore/ImageLoadController.h>

#include <string>

namespace OIV
{
    struct ImageLoadPresentation
    {
        bool succeeded = false;
        bool shouldLoadImage = false;
        bool shouldShowMessage = false;
        std::wstring message;
    };

    class ImageLoadPresentationPolicy
    {
      public:
        static ImageLoadPresentation Decide(const ImageLoadResult& loadResult,
                                            const std::wstring& formattedFilePath);
    };
}
