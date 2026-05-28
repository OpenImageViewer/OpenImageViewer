#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/CommandManager.h>
#include <OIVShared/FileSorter.h>

#include <string>

namespace OIV
{
    struct SortCommandDecision
    {
        bool valid                    = false;
        FileSorter::SortType sortType = FileSorter::SortType::Name;
        bool reverseDirection         = false;
    };

    class SortCommandPolicy
    {
      public:

        static SortCommandDecision Decide(const CommandManager::CommandArgs& args,
                                          FileSorter::SortType currentSortType);
        static FileSorter::SortDirection Reverse(FileSorter::SortDirection direction);
        static LLUtils::native_string_type FormatSortResult(const std::string& displayName,
                                                            FileSorter::SortDirection direction);
    };
}  // namespace OIV
