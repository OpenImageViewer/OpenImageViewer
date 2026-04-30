#include <oivappcore/SortCommandPolicy.h>

#include <LLUtils/StringUtility.h>

namespace OIV
{
    SortCommandDecision SortCommandPolicy::Decide(const CommandManager::CommandArgs& args,
                                                  FileSorter::SortType currentSortType)
    {
        const std::string sortType = args.GetArgValue("type");
        SortCommandDecision decision;

        if (sortType == "name")
            decision.sortType = FileSorter::SortType::Name;
        else if (sortType == "date")
            decision.sortType = FileSorter::SortType::Date;
        else if (sortType == "extension")
            decision.sortType = FileSorter::SortType::Extension;
        else
            return decision;

        decision.valid = true;
        decision.reverseDirection = decision.sortType == currentSortType;
        return decision;
    }

    FileSorter::SortDirection SortCommandPolicy::Reverse(FileSorter::SortDirection direction)
    {
        return direction == FileSorter::SortDirection::Ascending ? FileSorter::SortDirection::Descending
                                                                 : FileSorter::SortDirection::Ascending;
    }

    std::wstring SortCommandPolicy::FormatSortResult(const std::string& displayName,
                                                     FileSorter::SortDirection direction)
    {
        return LLUtils::StringUtility::ToWString(displayName) + L" [" +
               (direction == FileSorter::SortDirection::Ascending ? L"Ascending" : L"Descending") + L"]";
    }
}
