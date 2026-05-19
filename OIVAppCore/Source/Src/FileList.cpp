#include <OIVAppCore/FileList.h>

#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>

#include <algorithm>
#include <filesystem>
namespace OIV
{
    FileList::FileList(IFileListProvider* fileListProvider, IFileWatcher* fileWatcher, FileSorter* fileSorter,
                       const hashset_type& knownFileTypesSet, string_type knownfileTypes,
                       OnFileIndexChangedCallbackType callback)
        : fFileListProvider(fileListProvider), fFileWatcher(fileWatcher), fFileSorter(fileSorter),
          fKnownFileTypesSet(knownFileTypesSet), fKnownFileTypes(knownfileTypes), fOnFileIndexChangedCallback(callback)
    {
        fFileChangedConnection = fFileWatcher->GetFileChangedEvent().Connect(
            [this](IFileWatcher::FileChangedEventArgs fileChangedEventArgs) { OnFileChanged(fileChangedEventArgs); });
    }

    void FileList::Sort()
    {
        const string_type currentFile = IsIndexValid(fCurrentEntryIndex)
                                            ? fListFiles[static_cast<std::size_t>(fCurrentEntryIndex)]
                                            : fFileListProvider->GetActiveFileName();
        std::sort(fListFiles.begin(), fListFiles.end(), *fFileSorter);
        (void) SetCurrentIndexByElementName(currentFile);
    }

    const FileList::string_type& FileList::GetCurrentItemName() const
    {
        if (fListFiles.empty() || fCurrentEntryIndex == IndexStart || fCurrentEntryIndex == IndexEnd)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "No current item");

        return fListFiles.at(fCurrentEntryIndex);
    }

    size_t FileList::GetSize() const
    {
        return fListFiles.size();
    }

    FileList::Snapshot FileList::CreateSnapshot() const
    {
        return Snapshot{fCurrentFolder, fListFiles, fCurrentEntryIndex};
    }

    ResultCode FileList::SetCurrentIndexByElementName(const string_type& element)
    {
        list_string_type::iterator it = std::find(fListFiles.begin(), fListFiles.end(), element);

        if (it != fListFiles.end())
            return SetCurrentIndex(std::distance(fListFiles.begin(), it));

        return ResultCode::RC_OutOfRange;
    }

    FileList::index_type FileList::GetCurrentIndex() const
    {
        return fCurrentEntryIndex;
    }

    bool FileList::IsIndexValid(index_type index) const
    {
        return (index >= 0 && index < static_cast<index_type>(fListFiles.size()));
    }

    ResultCode FileList::SetCurrentIndex(index_type index)
    {
        if (fListFiles.empty())
            return ResultCode::RC_EmptyData;

        const index_type previousIndex = fCurrentEntryIndex;

        if (IsMarkerIndex(index) == true)
            index = GetIndexFromMarker(index);

        if (IsIndexValid(index) == true)
        {
            fCurrentEntryIndex = index;

            if (fOnFileIndexChangedCallback != nullptr && previousIndex != fCurrentEntryIndex)
                fOnFileIndexChangedCallback(fCurrentEntryIndex, previousIndex);

            return ResultCode::RC_Success;
        }
        else
        {
            return ResultCode::RC_OutOfRange;
        }
    }
    bool FileList::IsMarkerIndex(index_type index) const
    {
        return index == IndexStart || index == IndexEnd;
    }

    FileList::index_type FileList::GetIndexFromMarker(index_type marker) const
    {
        if (marker == IndexStart)
            return 0;
        else if (marker == IndexEnd)
            return static_cast<index_type>(fListFiles.size() - 1);
        else
            return marker;
    }

    IFileWatcher::FolderID FileList::GetFolderID() const
    {
        return fFolderID;
    }

    const FileList::string_type& FileList::GetFolder() const
    {
        return fCurrentFolder;
    }

    void FileList::SetFolder(const string_type& folder, list_string_type&& initialFileList)
    {
        using namespace std::filesystem;

        const std::wstring absoluteFolderPath = LLUtils::FileSystemHelper::ResolveFullPath(folder);

        if (absoluteFolderPath != fCurrentFolder)
        {
            if (fCurrentFolder.empty() == false)
                fFileWatcher->RemoveFolder(fCurrentFolder);

            fCurrentFolder = absoluteFolderPath;

            if (initialFileList.empty() == false)
            {
                fListFiles = std::move(initialFileList);
                std::sort(fListFiles.begin(), fListFiles.end(), *fFileSorter);
            }
            else
            {
                LoadFileInFolder();
            }
            // Watch current folder
            fFolderID = fFileWatcher->AddFolder(fCurrentFolder);

            UpdateEntryIndex();
        }
    }

    void FileList::UpdateEntryIndex()
    {
        // File has been added to the current folder, indices have changed - update current file index
        auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(),
                                              fFileListProvider->GetActiveFileName(), *fFileSorter);

        fCurrentEntryIndex = std::distance(fListFiles.begin(), itCurrentFile);
    }

    FileList::string_type FileList::GetElementNameFromIndex(index_type index) const
    {
        auto actualIndex = GetIndexFromMarker(index);
        if (IsIndexValid(actualIndex) == true)
            return fListFiles.at(actualIndex);
        else
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "Index is out of range");
    }

    void FileList::LoadFileInFolder()
    {
        using namespace std::filesystem;

        auto fileList = GetSupportedFileListInFolder(fCurrentFolder);
        // File is loaded from a different folder then the active one.
        std::swap(fListFiles, fileList);
    }

    FileList::list_string_type FileList::GetSupportedFileListInFolder(const FileList::string_type& folderPath)
    {
        FileList::list_string_type fileList;

        if (std::filesystem::is_directory(folderPath))
        {
            LLUtils::FileSystemHelper::FindFiles(fileList, folderPath, fKnownFileTypes, false, false);
        }
        else
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Not a folder");
        }

        return fileList;
    }

    void FileList::OnFileChanged(IFileWatcher::FileChangedEventArgs fileChangedEventArgs)  // callback from file watcher
    {
        if (fileChangedEventArgs.folderID == fFolderID)
        {
            auto openedFileName             = fFileListProvider->GetActiveFileName();
            std::wstring absoluteFilePath   = std::filesystem::path(openedFileName);
            std::wstring absoluteFolderPath = std::filesystem::path(openedFileName).parent_path();
            std::wstring changedFileName =
                (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName).wstring();
            std::wstring changedFileName2 =
                (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName2).wstring();

            switch (fileChangedEventArgs.fileOp)
            {
                case IFileWatcher::FileChangedOp::None:
                    break;
                case IFileWatcher::FileChangedOp::Add:
                    UpdateFileList(fileChangedEventArgs.fileOp, changedFileName, std::wstring());
                    break;
                case IFileWatcher::FileChangedOp::Remove:
                    UpdateFileList(fileChangedEventArgs.fileOp, changedFileName, std::wstring());
                    break;
                case IFileWatcher::FileChangedOp::Modified:
                    break;
                case IFileWatcher::FileChangedOp::Rename:
                    UpdateFileList(IFileWatcher::FileChangedOp::Rename, changedFileName, changedFileName2);
                    break;
                case IFileWatcher::FileChangedOp::WatchedFolderRemoved:
                    fCurrentFolder.clear();
                    break;
            }
        }
    }

    void FileList::UpdateFileList(IFileWatcher::FileChangedOp fileOp, const std::wstring& filePath,
                                  const std::wstring& filePath2)
    {
        switch (fileOp)
        {
            case IFileWatcher::FileChangedOp::Add:
            {
                // Add file to list only if it's a known file type
                std::wstring extension = LLUtils::StringUtility::ToLower(
                    std::filesystem::path(filePath).extension().wstring());

                std::wstring_view sv(extension);
                if (sv.empty() == false)
                    sv = sv.substr(1);

                if (fKnownFileTypesSet.contains(sv.data()))
                {
                    // TODO: add file sorted
                    auto itAddedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath, *fFileSorter);

                    if (itAddedFile != fListFiles.end() && *itAddedFile == filePath)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Trying to add an existing file");

                    fListFiles.insert(itAddedFile, filePath);

                    UpdateEntryIndex();
                }
            }
            break;

            case IFileWatcher::FileChangedOp::Remove:
            {
                auto it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
                if (it != fListFiles.end())
                {
                    auto fileNameToRemove = *it;
                    fListFiles.erase(it);
                }
            }
            break;
            case IFileWatcher::FileChangedOp::Rename:
            {
                auto it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
                if (it != fListFiles.end())
                {
                    auto fileNameToRemove = *it;
                    fListFiles.erase(it);
                    auto itRenamedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath2,
                                                          *fFileSorter);
                    fListFiles.insert(itRenamedFile, filePath2);

                    if (filePath == fFileListProvider->GetActiveFileName())
                    {
                    }
                    else
                    {
                        // File has been added to the current folder, indices have changed - update current file index
                        auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(),
                                                              fFileListProvider->GetActiveFileName(), *fFileSorter);
                        fCurrentEntryIndex = std::distance(fListFiles.begin(), itCurrentFile);
                    }
                }
                else
                {
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Invalid file removal request");
                }
            }

            break;

            case IFileWatcher::FileChangedOp::Modified:
            case IFileWatcher::FileChangedOp::None:
            case IFileWatcher::FileChangedOp::WatchedFolderRemoved:
                break;
        }
    }

}  // namespace OIV
