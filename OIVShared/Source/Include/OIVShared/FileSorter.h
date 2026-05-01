#pragma once
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>

#include <array>
#include <filesystem>
#include <string>
#include <system_error>
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
                std::error_code errorA;
                std::error_code errorB;
                const auto timeA = std::filesystem::last_write_time(A, errorA);
                const auto timeB = std::filesystem::last_write_time(B, errorB);
                if (errorA || errorB)
                    return direction == SortDirection::Ascending ? A < B : B < A;

                return direction == SortDirection::Ascending ? timeA < timeB : timeB < timeA;
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
