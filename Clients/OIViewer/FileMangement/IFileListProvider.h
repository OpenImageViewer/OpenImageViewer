#pragma once

#include <LLUtils/StringDefs.h>

namespace OIV
{
    using FileListStringType = LLUtils::native_string_type;
    using FileListStringSetType = std::set<FileListStringType>;

    class IFileListProvider
    {
      public:
        virtual FileListStringType GetActiveFileName() = 0;
    };
}  // namespace OIV