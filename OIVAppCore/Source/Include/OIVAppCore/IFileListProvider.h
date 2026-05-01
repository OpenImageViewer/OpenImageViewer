#pragma once

#include <LLUtils/StringDefs.h>

#include <set>

namespace OIV
{
    using FileListStringType = LLUtils::native_string_type;
    using FileListStringSetType = std::set<FileListStringType>;

    class IFileListProvider
    {
      public:
        virtual ~IFileListProvider() = default;
        virtual FileListStringType GetActiveFileName() = 0;
    };
}  // namespace OIV
