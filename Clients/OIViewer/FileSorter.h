#pragma once
namespace OIV
{
    class FileSorter
    {
    public:
        enum class SortType
        {
            Name,
            Date
        };

        enum class SortDirection
        {
            Ascending,
            Descending
        };

    private:
        struct FileNameSorter
        {
            bool operator() (const std::wstring& A, const std::wstring& B, SortDirection direction) const
            {
                using namespace LLUtils;
                using path = std::filesystem::path;
                path aPath(StringUtility::ToLower(A));
                std::wstring aName = aPath.stem();
                std::wstring aExt = aPath.extension();

                path bPath(StringUtility::ToLower(B));
                std::wstring bName = bPath.stem();
                std::wstring bExt = bPath.extension();

                return direction == SortDirection::Ascending ? (aName < bName || ((aName == bName) && aExt < bExt))
                    : bName < aName || ((bName == aName) && bExt < aExt);
            }
        } fFileListNameSorter;

        struct FileDateSorter
        {
            bool operator() (const std::wstring& A, const std::wstring& B, SortDirection direction) const
            {
                using namespace LLUtils;
                using namespace std::filesystem;

                return direction == SortDirection::Ascending ? last_write_time(A) < last_write_time(B)
                    : last_write_time(B) < last_write_time(A);
            }
        }  fFileListDateSorter;
    public:
        bool operator() (const std::wstring& A, const std::wstring& B) const
        {
            switch (fSortType)
            {
            case SortType::Date:
                return fFileListDateSorter(A, B, fSortDirection);
                break;
            case SortType::Name:
                return fFileListNameSorter(A, B, fSortDirection);
                break;
            }
        }

        void SetSortType(SortType sortType)
        {
            fSortType = sortType;
        }

        SortType GetSortType() const
        {
            return fSortType;
        }
            SortDirection GetSortDirection() const
        {
            return fSortDirection;
        }

        void SetSortDirection(SortDirection sortDirection)
        {
            fSortDirection = sortDirection;
        }

    private:
        SortType fSortType = SortType::Name;
        SortDirection fSortDirection = SortDirection::Ascending;
    };
}