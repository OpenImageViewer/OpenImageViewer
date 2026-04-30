#pragma once

#include <oivappcore/FileList.h>
#include <oivshared/BrowseResidencyManager.h>
#include <oivshared/ImageResidency.h>

#include <functional>
#include <memory>

namespace OIV
{
    class FileSessionController
    {
      public:
        using CurrentImageReadyCallback = BrowseResidencyManager::CurrentImageReadyCallback;
        using FolderLoadReadyCallback = BrowseResidencyManager::FolderLoadReadyCallback;

        FileSessionController(IFileListProvider* fileListProvider,
                              IFileWatcher* fileWatcher,
                              FileSorter* fileSorter,
                              const FileList::hashset_type& knownFileTypesSet,
                              FileList::string_type knownFileTypes,
                              ImageResidency& imageResidency,
                              CurrentImageReadyCallback currentImageReadyCallback,
                              FolderLoadReadyCallback folderLoadReadyCallback);

        FileList& GetFileList();
        const FileList& GetFileList() const;

        void SortFileList();
        void LoadFileInFolder(const std::wstring& absoluteFilePath);
        void PrepareDirectFileLoad(const std::wstring& normalizedFilePath);
        void InvalidateCurrent();
        bool JumpFiles(FileList::index_type step);
        bool RequestFolderLoadResidency(const std::wstring& folderPath);
        bool IsCurrentFile(const std::wstring& fileName) const;
        bool OnFolderLoadResidencyReady(const BrowseResidencyManager::FileListSnapshot& snapshot,
                                        const std::wstring& fileName,
                                        IMCodec::ImageSharedPtr image);
        void RequestCurrentFileReload();

      private:
        void OnFileIndexChanged(FileList::index_type current, FileList::index_type previous);

        std::unique_ptr<FileList> fFileList;
        BrowseResidencyManager fBrowseResidencyManager;
    };
}  // namespace OIV
