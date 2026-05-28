#include <LLUtils/StringDefs.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>

namespace OIV
{
    ImageLoadPresentation ImageLoadPresentationPolicy::Decide(const ImageLoadResult& loadResult,
                                                              const LLUtils::native_string_type& formattedFilePath)
    {
        using namespace std::string_literals;

        switch (loadResult.status)
        {
            case ImageLoadStatus::FolderLoadQueued:
                return {true, false, false, {}};
            case ImageLoadStatus::NoSupportedFiles:
                return {false, false, false, {}};
            case ImageLoadStatus::Loaded:
                return {loadResult.DecodeSucceeded(), true, false, {}};
            case ImageLoadStatus::TooLarge:
                return {false, false, true,
                        LLUTILS_TEXT("Can not load the file: "s) + formattedFilePath +
                            LLUTILS_TEXT(", image dimensions are more than 16384: ")};
            case ImageLoadStatus::UnsupportedFormat:
                return {false, false, true,
                        LLUTILS_TEXT("Can not load the file: "s) + formattedFilePath +
                            LLUTILS_TEXT(", image format is not supported"s)};
            case ImageLoadStatus::UnknownError:
            default:
                return {false, false, true,
                        LLUTILS_TEXT("Can not load the file: "s) + formattedFilePath + LLUTILS_TEXT(", unkown error"s)};
        }
    }
}  // namespace OIV
