#include <oivappcore/FileSessionController.h>

#include <filesystem>

namespace OIV
{
    FileSessionController::FileSessionController(IFileListProvider* fileListProvider,
                                                 IFileWatcher* fileWatcher,
                                                 FileSorter* fileSorter,
                                                 const FileList::hashset_type& knownFileTypesSet,
                                                 FileList::string_type knownFileTypes,
                                                 ImageResidency& imageResidency,
                                                 CurrentImageReadyCallback currentImageReadyCallback,
                                                 FolderLoadReadyCallback folderLoadReadyCallback)
        : fFileList(std::make_unique<FileList>(
              fileListProvider,
              fileWatcher,
              fileSorter,
              knownFileTypesSet,
              std::move(knownFileTypes),
              [this](FileList::index_type current, FileList::index_type previous)
              {
                  OnFileIndexChanged(current, previous);
              }))
        , fBrowseResidencyManager(imageResidency,
                                  std::move(currentImageReadyCallback),
                                  std::move(folderLoadReadyCallback))
    {
    }

    FileList& FileSessionController::GetFileList()
    {
        return *fFileList;
    }

    const FileList& FileSessionController::GetFileList() const
    {
        return *fFileList;
    }

    void FileSessionController::SortFileList()
    {
        fFileList->Sort();
    }

    void FileSessionController::LoadFileInFolder(const std::wstring& absoluteFilePath)
    {
        fFileList->SetFolder(std::filesystem::path(absoluteFilePath).parent_path(), {});
    }

    void FileSessionController::PrepareDirectFileLoad(const std::wstring& normalizedFilePath)
    {
        fBrowseResidencyManager.SetWorkingFolder(std::filesystem::path(normalizedFilePath).parent_path().wstring());
        fBrowseResidencyManager.InvalidateCurrent();
    }

    void FileSessionController::InvalidateCurrent()
    {
        fBrowseResidencyManager.InvalidateCurrent();
    }

    bool FileSessionController::JumpFiles(FileList::index_type step)
    {
        auto initialIndex = fFileList->GetCurrentIndex();
        auto targetIndex = fFileList->IsMarkerIndex(step) ? step : initialIndex + step;
        auto res = fFileList->SetCurrentIndex(targetIndex);

        if (res == ResultCode::RC_EmptyData)
            return false;

        if (res == ResultCode::RC_OutOfRange)
            res = fFileList->SetCurrentIndex(step > 0 ? FileList::IndexStart : FileList::IndexEnd);

        return res == ResultCode::RC_Success;
    }

    bool FileSessionController::RequestFolderLoadResidency(const std::wstring& folderPath)
    {
        auto fileList = fFileList->GetSupportedFileListInFolder(folderPath);
        if (fileList.empty())
            return false;

        const auto normalizedFolderPath = std::filesystem::path(folderPath).lexically_normal().wstring();
        fBrowseResidencyManager.RequestFolderLoadResidency(
            BrowseResidencyManager::FileListSnapshot{
                normalizedFolderPath,
                fileList,
                BrowseResidencyManager::FileListSnapshot::IndexStart});
        return true;
    }

    bool FileSessionController::IsCurrentFile(const std::wstring& fileName) const
    {
        return fFileList != nullptr && fFileList->IsIndexValid(fFileList->GetCurrentIndex()) &&
               fFileList->GetCurrentItemName() == fileName;
    }

    bool FileSessionController::OnFolderLoadResidencyReady(const BrowseResidencyManager::FileListSnapshot& snapshot,
                                                           const std::wstring& fileName,
                                                           IMCodec::ImageSharedPtr image)
    {
        if (image == nullptr)
            return false;

        auto fileListCopy = snapshot.files;
        fFileList->SetFolder(snapshot.folderPath, std::move(fileListCopy));

        const auto previousIndex = fFileList->GetCurrentIndex();
        fFileList->SetCurrentIndexByElementName(fileName);

        if (previousIndex == fFileList->GetCurrentIndex())
            OnFileIndexChanged(fFileList->GetCurrentIndex(), fFileList->GetCurrentIndex());

        return true;
    }

    void FileSessionController::RequestCurrentFileReload()
    {
        fBrowseResidencyManager.OnCurrentFileReloadRequested(fFileList->CreateSnapshot());
    }

    void FileSessionController::OnFileIndexChanged(FileList::index_type current, FileList::index_type previous)
    {
        (void) current;
        fBrowseResidencyManager.OnCurrentIndexChanged(fFileList->CreateSnapshot(), previous);
    }
}  // namespace OIV
