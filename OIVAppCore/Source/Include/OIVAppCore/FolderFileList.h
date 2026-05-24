#pragma once

#include <LLUtils/StringDefs.h>
#include <LLUtils/FileSystemHelper.h>
#include <OIVAppCore/IFileWatcher.h>
#include <OIVShared/FileSorter.h>

#include <functional>
#include <limits>
#include <iterator>
#include <set>
#include <Defs.h>

namespace OIV
{
    using FolderFileListStringType    = LLUtils::native_string_type;
    using FolderFileListStringSetType = std::set<FolderFileListStringType>;

    /// @brief A class to manage folder file order and current index.
    class FolderFileList
    {
      public:

        using string_type                    = FolderFileListStringType;
        using list_string_type               = LLUtils::ListString<string_type>;
        using hashset_type                   = std::set<string_type>;
        using index_type                     = list_string_type::difference_type;
        using size_type                      = list_string_type::size_type;
        using OnFileIndexChangedCallbackType = std::function<void(index_type current, index_type previous)>;

        static constexpr auto IndexEnd   = std::numeric_limits<index_type>::max();
        static constexpr auto IndexStart = std::numeric_limits<index_type>::min();

        struct Snapshot
        {
            string_type folderPath;
            list_string_type files;
            index_type currentIndex = IndexStart;
        };

        FolderFileList(FileSorter* fileSorter, const hashset_type& knownFileTypesSet, string_type knownfileTypes,
                       OnFileIndexChangedCallbackType callback);

        index_type GetCurrentIndex() const;
        ResultCode SetCurrentIndex(index_type index);
        bool IsMarkerIndex(index_type index) const;
        index_type GetIndexFromMarker(index_type marker) const;
        const string_type& GetCurrentItemName() const;
        bool IsIndexValid(index_type index) const;
        void SetFolder(const string_type& folder, list_string_type&& initialFolderFileList,
                       const string_type& currentFile);
        const string_type& GetFolder() const;
        size_t GetSize() const;
        Snapshot CreateSnapshot() const;
        ResultCode SetCurrentIndexByElementName(const string_type& element);
        void InvalidateCurrentIndex();
        string_type GetElementNameFromIndex(index_type index) const;
        void Sort(const string_type& currentFile);
        list_string_type GetSupportedFolderFileListInFolder(const string_type& folderPath);
        bool IsSupportedFileType(const string_type& filePath) const;
        void UpdateFolderFileList(IFileWatcher::FileChangedOp fileOp, const std::wstring& filePath,
                                  const std::wstring& filePath2, const string_type& currentFile);

      private:

        void UpdateEntryIndex(const string_type& currentFile);
        void LoadFilesInFolder();

      private:

        list_string_type fListFiles;
        index_type fCurrentEntryIndex = IndexStart;
        string_type fCurrentFolder;
        FileSorter* fFileSorter;
        hashset_type fKnownFileTypesSet;
        string_type fKnownFileTypes;
        OnFileIndexChangedCallbackType fOnFileIndexChangedCallback;
    };
}  // namespace OIV
