#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/ImageOpenController.h>

#include <string>

namespace OIV
{
    struct ImageLoadPresentation
    {
        bool succeeded         = false;
        bool shouldLoadImage   = false;
        bool shouldShowMessage = false;
        LLUtils::native_string_type message;
    };

    class ImageLoadPresentationPolicy
    {
      public:

        static ImageLoadPresentation Decide(const ImageLoadResult& loadResult,
                                            const LLUtils::native_string_type& formattedFilePath);
    };
}  // namespace OIV
