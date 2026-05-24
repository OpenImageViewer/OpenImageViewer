#include <OIVAppCore/FolderFileList.h>

#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>

#include <algorithm>
#include <filesystem>
namespace OIV
{
    FolderFileList::FolderFileList(FileSorter* fileSorter, const hashset_type& knownFileTypesSet,
                                   string_type knownfileTypes, OnFileIndexChangedCallbackType callback)
        : fFileSorter(fileSorter), fKnownFileTypesSet(knownFileTypesSet), fKnownFileTypes(knownfileTypes),
          fOnFileIndexChangedCallback(callback)
    {
    }

    void FolderFileList::Sort(const string_type& currentFile)
    {
        std::sort(fListFiles.begin(), fListFiles.end(), *fFileSorter);
        (void) SetCurrentIndexByElementName(currentFile);
    }

    const FolderFileList::string_type& FolderFileList::GetCurrentItemName() const
    {
        if (fListFiles.empty() || fCurrentEntryIndex == IndexStart || fCurrentEntryIndex == IndexEnd)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "No current item");

        return fListFiles.at(fCurrentEntryIndex);
    }

    size_t FolderFileList::GetSize() const
    {
        return fListFiles.size();
    }

    FolderFileList::Snapshot FolderFileList::CreateSnapshot() const
    {
        return Snapshot{fCurrentFolder, fListFiles, fCurrentEntryIndex};
    }

    ResultCode FolderFileList::SetCurrentIndexByElementName(const string_type& element)
    {
        list_string_type::iterator it = std::find(fListFiles.begin(), fListFiles.end(), element);

        if (it != fListFiles.end())
            return SetCurrentIndex(std::distance(fListFiles.begin(), it));

        return ResultCode::RC_OutOfRange;
    }

    FolderFileList::index_type FolderFileList::GetCurrentIndex() const
    {
        return fCurrentEntryIndex;
    }

    bool FolderFileList::IsIndexValid(index_type index) const
    {
        return (index >= 0 && index < static_cast<index_type>(fListFiles.size()));
    }

    ResultCode FolderFileList::SetCurrentIndex(index_type index)
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
    bool FolderFileList::IsMarkerIndex(index_type index) const
    {
        return index == IndexStart || index == IndexEnd;
    }

    FolderFileList::index_type FolderFileList::GetIndexFromMarker(index_type marker) const
    {
        if (marker == IndexStart)
            return 0;
        else if (marker == IndexEnd)
            return static_cast<index_type>(fListFiles.size() - 1);
        else
            return marker;
    }

    const FolderFileList::string_type& FolderFileList::GetFolder() const
    {
        return fCurrentFolder;
    }

    void FolderFileList::SetFolder(const string_type& folder, list_string_type&& initialFolderFileList,
                                   const string_type& currentFile)
    {
        using namespace std::filesystem;

        const std::wstring absoluteFolderPath = LLUtils::FileSystemHelper::ResolveFullPath(folder);

        if (absoluteFolderPath != fCurrentFolder)
        {
            fCurrentFolder = absoluteFolderPath;

            if (initialFolderFileList.empty() == false)
            {
                fListFiles = std::move(initialFolderFileList);
                std::sort(fListFiles.begin(), fListFiles.end(), *fFileSorter);
            }
            else
            {
                LoadFilesInFolder();
            }
        }

        UpdateEntryIndex(currentFile);
    }

    void FolderFileList::UpdateEntryIndex(const string_type& currentFile)
    {
        auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), currentFile, *fFileSorter);

        if (itCurrentFile != fListFiles.end() && *itCurrentFile == currentFile)
            fCurrentEntryIndex = std::distance(fListFiles.begin(), itCurrentFile);
        else
            InvalidateCurrentIndex();
    }

    FolderFileList::string_type FolderFileList::GetElementNameFromIndex(index_type index) const
    {
        auto actualIndex = GetIndexFromMarker(index);
        if (IsIndexValid(actualIndex) == true)
            return fListFiles.at(actualIndex);
        else
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "Index is out of range");
    }

    void FolderFileList::LoadFilesInFolder()
    {
        using namespace std::filesystem;

        auto fileList = GetSupportedFolderFileListInFolder(fCurrentFolder);
        std::sort(fileList.begin(), fileList.end(), *fFileSorter);
        // File is loaded from a different folder then the active one.
        std::swap(fListFiles, fileList);
    }

    FolderFileList::list_string_type FolderFileList::GetSupportedFolderFileListInFolder(
        const FolderFileList::string_type& folderPath)
    {
        FolderFileList::list_string_type fileList;

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

    void FolderFileList::InvalidateCurrentIndex()
    {
        fCurrentEntryIndex = IndexStart;
    }

    bool FolderFileList::IsSupportedFileType(const string_type& filePath) const
    {
        std::wstring extension = LLUtils::StringUtility::ToLower(std::filesystem::path(filePath).extension().wstring());

        std::wstring_view sv(extension);
        if (sv.empty() == false)
            sv = sv.substr(1);

        return fKnownFileTypesSet.contains(sv.data());
    }

    void FolderFileList::UpdateFolderFileList(IFileWatcher::FileChangedOp fileOp, const std::wstring& filePath,
                                              const std::wstring& filePath2, const string_type& currentFile)
    {
        bool shouldUpdateIndex = false;
        switch (fileOp)
        {
            case IFileWatcher::FileChangedOp::Add:
            {
                // Add file to list only if it's a known file type
                if (IsSupportedFileType(filePath))
                {
                    // TODO: add file sorted
                    auto itAddedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath, *fFileSorter);

                    if (itAddedFile != fListFiles.end() && *itAddedFile == filePath)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Trying to add an existing file");

                    fListFiles.insert(itAddedFile, filePath);

                    shouldUpdateIndex = true;
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
                    shouldUpdateIndex = true;
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
                    shouldUpdateIndex = true;
                }

                if (IsSupportedFileType(filePath2))
                {
                    auto itRenamedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath2,
                                                          *fFileSorter);
                    if (itRenamedFile != fListFiles.end() && *itRenamedFile == filePath2)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Trying to add an existing file");

                    fListFiles.insert(itRenamedFile, filePath2);
                    shouldUpdateIndex = true;
                }
            }

            break;

            case IFileWatcher::FileChangedOp::Modified:
            case IFileWatcher::FileChangedOp::None:
            case IFileWatcher::FileChangedOp::WatchedFolderRemoved:
                break;
        }

        if (shouldUpdateIndex)
            UpdateEntryIndex(currentFile);
    }

}  // namespace OIV
