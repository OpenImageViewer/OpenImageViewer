#pragma once
namespace OIV
{
    class FileSorter
    {
    public:
        enum class SortType
        {
              Name
            , Date
            , Extension
            , Count

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

        struct FileExtensionSorter
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

                return direction == SortDirection::Ascending ? (aExt < bExt || ((aExt == bExt) && aName < bName))
                    : bExt < aExt || ((bExt == aExt) && bName < aName);
            }
        } fFileListExtensionSorter;

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
                return fFileListDateSorter(A, B, GetActiveSortDirection());
                break;
            case SortType::Name:
                return fFileListNameSorter(A, B, GetActiveSortDirection());
                break;
            case SortType::Extension:
                return fFileListExtensionSorter(A, B, GetActiveSortDirection());
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
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
        
        SortDirection GetActiveSortDirection() const
        {
            return fSortDirection[static_cast<size_t>(fSortType)];
        }

        void SetSortDirection(SortType sortType,  SortDirection sortDirection)
        {
            fSortDirection[static_cast<size_t>(sortType)] = sortDirection;
        }

        void SetActiveSortDirection(SortDirection sortDirection)
        {
            SetSortDirection(fSortType, sortDirection);
        }

    private:
        SortType fSortType = SortType::Name;
        std::array<SortDirection, static_cast<size_t>(SortType::Count)> fSortDirection{ SortDirection::Ascending , SortDirection::Descending, SortDirection::Ascending };
    };
}