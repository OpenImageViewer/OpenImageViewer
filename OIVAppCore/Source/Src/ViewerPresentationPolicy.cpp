#include <LLUtils/StringDefs.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>

#include <sstream>
#include <string_view>

namespace
{
    bool IsPathSeparator(LLUtils::native_char_type ch)
    {
        return ch == static_cast<LLUtils::native_char_type>('/') || ch == static_cast<LLUtils::native_char_type>('\\');
    }

    LLUtils::native_string_view TrimTrailingPathSeparator(const LLUtils::native_string_type& path)
    {
        auto trimmedPath = LLUtils::native_string_view(path);
        if (trimmedPath.empty() == false && IsPathSeparator(trimmedPath.back()))
            trimmedPath.remove_suffix(1);

        return trimmedPath;
    }
}  // namespace

namespace OIV
{
    LLUtils::native_string_type ViewerPresentationPolicy::FormatOperationResult(OperationResult result)
    {
        switch (result)
        {
            case OperationResult::NoDataFound:
                return LLUTILS_TEXT("No Image loaded");
            case OperationResult::Success:
                return LLUTILS_TEXT("Success");
            case OperationResult::NoSelection:
                return LLUTILS_TEXT("No selection");
            case OperationResult::UnkownError:
            default:
                return LLUTILS_TEXT("Unknown error");
        }
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatFailedOperation(
        const LLUtils::native_string_type& action, OperationResult result)
    {
        return action + LLUTILS_TEXT(" - ") + FormatOperationResult(result);
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatOpenedFileMessage(
        const LLUtils::native_string_type& formattedFilePath)
    {
        return LLUTILS_TEXT("File: ") + formattedFilePath;
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatTopMostMessage(int counter)
    {
        LLUtils::native_stringstream stream;
        stream << LLUTILS_TEXT("Top most ending in...") << counter;
        return stream.str();
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatNonFileTitlePrefix(ImageSource source)
    {
        switch (source)
        {
            case ImageSource::ClipboardText:
                return LLUTILS_TEXT("Clipboard text - ");
            case ImageSource::Clipboard:
                return LLUTILS_TEXT("Clipboard image - ");
            case ImageSource::GeneratedByLib:
                return LLUTILS_TEXT("Internal image - ");
            default:
                return LLUTILS_TEXT("Unknown image source - ");
        }
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatFileTitlePrefix(
        const LLUtils::native_string_type& fileName, const LLUtils::native_string_type& extension,
        const LLUtils::native_string_type& parentPath, bool includeIndex, size_t displayIndex, size_t fileCount)
    {
        LLUtils::native_stringstream stream;
        if (includeIndex)
            stream << displayIndex << LLUTILS_TEXT("/") << fileCount << LLUTILS_TEXT(" | ");

        stream << fileName << extension << LLUTILS_TEXT(" @ ");
        if (parentPath.empty() == false)
            stream << TrimTrailingPathSeparator(parentPath);

        stream << LLUTILS_TEXT(" - ");
        return stream.str();
    }

    LLUtils::native_string_type ViewerPresentationPolicy::FormatTitle(const LLUtils::native_string_type& imagePrefix,
                                                                      const LLUtils::native_string_type& versionText)
    {
        return imagePrefix + versionText;
    }
}  // namespace OIV
