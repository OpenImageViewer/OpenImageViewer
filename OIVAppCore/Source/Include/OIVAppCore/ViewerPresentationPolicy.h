#pragma once

#include <LLUtils/StringDefs.h>

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

        static LLUtils::native_string_type FormatOperationResult(OperationResult result);
        static LLUtils::native_string_type FormatFailedOperation(const LLUtils::native_string_type& action,
                                                                 OperationResult result);
        static LLUtils::native_string_type FormatOpenedFileMessage(const LLUtils::native_string_type& formattedFilePath);
        static LLUtils::native_string_type FormatTopMostMessage(int counter);
        static LLUtils::native_string_type FormatNonFileTitlePrefix(ImageSource source);
        static LLUtils::native_string_type FormatFileTitlePrefix(const LLUtils::native_string_type& fileName,
                                                                 const LLUtils::native_string_type& extension,
                                                                 const LLUtils::native_string_type& parentPath,
                                                                 bool includeIndex, size_t displayIndex,
                                                                 size_t fileCount);
        static LLUtils::native_string_type FormatTitle(const LLUtils::native_string_type& imagePrefix,
                                                       const LLUtils::native_string_type& versionText);
    };
}  // namespace OIV
