#pragma once

#include <OIVImage/OIVBaseImage.h>

#include <string>

namespace OIV
{
    enum class OperationResult
    {
        Success,
        NoDataFound,
        NoSelection,
        UnkownError
    };

    class ViewerPresentationPolicy
    {
      public:
        static std::wstring FormatOperationResult(OperationResult result);
        static std::wstring FormatFailedOperation(const std::wstring& action, OperationResult result);
        static std::wstring FormatOpenedFileMessage(const std::wstring& formattedFilePath);
        static std::wstring FormatTopMostMessage(int counter);
        static std::wstring FormatNonFileTitlePrefix(ImageSource source);
        static std::wstring FormatFileTitlePrefix(const std::wstring& fileName,
                                                  const std::wstring& extension,
                                                  const std::wstring& parentPath,
                                                  bool includeIndex,
                                                  size_t displayIndex,
                                                  size_t fileCount);
        static std::wstring FormatTitle(const std::wstring& imagePrefix, const std::wstring& versionText);
    };
}  // namespace OIV
