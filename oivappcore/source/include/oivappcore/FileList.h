#pragma once

#include <LLUtils/StringDefs.h>
#include <LLUtils/FileSystemHelper.h>
#include <oivappcore/IFileListProvider.h>
#include <oivappcore/IFileWatcher.h>
#include <oivshared/FileSorter.h>

#include <functional>
#include <limits>
#include <iterator>
#include <set>
#include <defs.h>

namespace OIV
{
    /// @brief A class to manage file browsing with embedded file watcher
    class FileList
    {
      public:

        using string_type = FileListStringType;
        using list_string_type = LLUtils::ListString<string_type>;
        using hashset_type = std::set<string_type>;
        using index_type = list_string_type::difference_type;
        using size_type = list_string_type::size_type;
        using OnFileIndexChangedCallbackType = std::function<void(index_type current, index_type previous)>;

        static constexpr auto IndexEnd = std::numeric_limits<index_type>::max();
        static constexpr auto IndexStart = std::numeric_limits<index_type>::min();

        struct Snapshot
        {
            string_type folderPath;
            list_string_type files;
            index_type currentIndex = IndexStart;
        };

        FileList(IFileListProvider* fileListProvider, IFileWatcher* fileWatcher, FileSorter* fileSorter,
                 const hashset_type& knownFileTypesSet, string_type knownfileTypes, OnFileIndexChangedCallbackType callback);

        index_type GetCurrentIndex() const;
        ResultCode SetCurrentIndex(index_type index);
        bool IsMarkerIndex(index_type index) const;
        index_type GetIndexFromMarker(index_type marker) const;
        const string_type& GetCurrentItemName() const;
        bool IsIndexValid(index_type index) const;
        void SetFolder(const string_type& folder, list_string_type&& initialFileList);
        const string_type& GetFolder() const;
        IFileWatcher::FolderID GetFolderID() const;
        size_t GetSize() const;
        Snapshot CreateSnapshot() const;
        void SetCurrentIndexByElementName(const string_type& element);
        string_type GetElementNameFromIndex(index_type index) const;
        void Sort();
        list_string_type GetSupportedFileListInFolder(const string_type& folderPath);

      private:

        void UpdateEntryIndex();
        void OnFileChanged(IFileWatcher::FileChangedEventArgs fileChangedEventArgs);  // callback from file watcher
        void UpdateFileList(IFileWatcher::FileChangedOp fileOp, const std::wstring& filePath,
                            const std::wstring& filePath2);
        void LoadFileInFolder();

      private:

        IFileListProvider* fFileListProvider{};
        list_string_type fListFiles;
        index_type fCurrentEntryIndex = IndexStart;
        string_type fCurrentFolder;
        IFileWatcher::FolderID fFolderID{};
        IFileWatcher* fFileWatcher{};
        FileSorter* fFileSorter;
        hashset_type fKnownFileTypesSet;
        string_type fKnownFileTypes;
        OnFileIndexChangedCallbackType fOnFileIndexChangedCallback;
    };
}  // namespace OIV
