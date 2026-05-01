#include <OIVAppCore/ViewerPresentationPolicy.h>

#include <sstream>
#include <string_view>

namespace OIV
{
    std::wstring ViewerPresentationPolicy::FormatOperationResult(OperationResult result)
    {
        switch (result)
        {
            case OperationResult::NoDataFound:
                return L"No Image loaded";
            case OperationResult::Success:
                return L"Success";
            case OperationResult::NoSelection:
                return L"No selection";
            case OperationResult::UnkownError:
            default:
                return L"Unknown error";
        }
    }

    std::wstring ViewerPresentationPolicy::FormatFailedOperation(const std::wstring& action, OperationResult result)
    {
        return action + L" - " + FormatOperationResult(result);
    }

    std::wstring ViewerPresentationPolicy::FormatOpenedFileMessage(const std::wstring& formattedFilePath)
    {
        return L"File: " + formattedFilePath;
    }

    std::wstring ViewerPresentationPolicy::FormatTopMostMessage(int counter)
    {
        return L"Top most ending in..." + std::to_wstring(counter);
    }

    std::wstring ViewerPresentationPolicy::FormatNonFileTitlePrefix(ImageSource source)
    {
        switch (source)
        {
            case ImageSource::ClipboardText:
                return L"Clipboard text - ";
            case ImageSource::Clipboard:
                return L"Clipboard image - ";
            case ImageSource::GeneratedByLib:
                return L"Internal image - ";
            default:
                return L"Unknown image source - ";
        }
    }

    std::wstring ViewerPresentationPolicy::FormatFileTitlePrefix(const std::wstring& fileName,
                                                                 const std::wstring& extension,
                                                                 const std::wstring& parentPath,
                                                                 bool includeIndex,
                                                                 size_t displayIndex,
                                                                 size_t fileCount)
    {
        std::wstringstream stream;
        if (includeIndex)
            stream << displayIndex << L"/" << fileCount << L" | ";

        stream << fileName << extension << L" @ ";
        if (parentPath.empty() == false)
            stream << std::wstring_view(parentPath.data(), parentPath.length() - 1);

        stream << L" - ";
        return stream.str();
    }

    std::wstring ViewerPresentationPolicy::FormatTitle(const std::wstring& imagePrefix,
                                                       const std::wstring& versionText)
    {
        return imagePrefix + versionText;
    }
}  // namespace OIV
