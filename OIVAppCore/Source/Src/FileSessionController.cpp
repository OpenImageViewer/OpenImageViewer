#include <OIVAppCore/FileSessionController.h>

#include <filesystem>
#include <utility>

namespace OIV
{
    namespace
    {
        class ScopedFileIndexChangeSuppression
        {
          public:

            explicit ScopedFileIndexChangeSuppression(bool& suppressFileIndexChanged)
                : fSuppressFileIndexChanged(suppressFileIndexChanged)
            {
                fSuppressFileIndexChanged = true;
            }

            ~ScopedFileIndexChangeSuppression() { fSuppressFileIndexChanged = false; }

          private:

            bool& fSuppressFileIndexChanged;
        };
    }  // namespace

    FileSessionController::FileSessionController(IFileListProvider* fileListProvider, IFileWatcher* fileWatcher,
                                                 FileSorter* fileSorter,
                                                 const FileList::hashset_type& knownFileTypesSet,
                                                 FileList::string_type knownFileTypes, ImageResidency& imageResidency,
                                                 CurrentImageReadyCallback currentImageReadyCallback,
                                                 FolderLoadReadyCallback folderLoadReadyCallback,
                                                 CandidateImageReadyCallback candidateImageReadyCallback)
        : fFileList(std::make_unique<FileList>(fileListProvider, fileWatcher, fileSorter, knownFileTypesSet,
                                               std::move(knownFileTypes),
                                               [this](FileList::index_type current, FileList::index_type previous)
                                               { OnFileIndexChanged(current, previous); })),
          fCurrentImageReadyCallback(std::move(currentImageReadyCallback)),
          fCandidateImageReadyCallback(std::move(candidateImageReadyCallback)),
          fBrowseResidencyManager(
              imageResidency,
              [this](const std::wstring& fileName, IMCodec::ImageSharedPtr image)
              {
                  if (fCurrentImageReadyCallback)
                      fCurrentImageReadyCallback(fileName, std::move(image));
              },
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
        // Sorting changes navigation order, so pending candidate requests from the previous order must not commit or
        // continue scanning after they complete asynchronously.
        ClearPendingNavigation();

        const auto previousIndex = fFileList->GetCurrentIndex();
        fFileList->Sort();
        const auto currentIndex = fFileList->GetCurrentIndex();
        if (previousIndex == currentIndex && fFileList->IsIndexValid(currentIndex))
            OnFileIndexChanged(currentIndex, previousIndex);
    }

    void FileSessionController::LoadFileInFolder(const std::wstring& absoluteFilePath)
    {
        fFileList->SetFolder(std::filesystem::path(absoluteFilePath).parent_path(), {});
        (void) fFileList->SetCurrentIndexByElementName(
            std::filesystem::path(absoluteFilePath).lexically_normal().wstring());
    }

    void FileSessionController::PrepareDirectFileLoad(const std::wstring& normalizedFilePath)
    {
        ClearPendingNavigation();
        fBrowseResidencyManager.SetWorkingFolder(std::filesystem::path(normalizedFilePath).parent_path().wstring());
        fBrowseResidencyManager.InvalidateCurrent();
    }

    void FileSessionController::InvalidateCurrent()
    {
        ClearPendingNavigation();
        fBrowseResidencyManager.InvalidateCurrent();
    }

    bool FileSessionController::JumpFiles(FileList::index_type step)
    {
        if (fFileList->GetSize() == 0)
            return false;

        const FileList::index_type currentIndex = fFileList->GetCurrentIndex();
        FileList::index_type requestedIndex     = step;
        int direction                           = 1;
        if (fFileList->IsMarkerIndex(step))
        {
            requestedIndex = fFileList->GetIndexFromMarker(step);
            direction      = requestedIndex >= currentIndex ? 1 : -1;
        }
        else
        {
            requestedIndex = currentIndex + step;
            direction      = step >= 0 ? 1 : -1;
        }

        return StartPendingNavigation(fFileList->GetFolder(), fFileList->CreateSnapshot().files, requestedIndex,
                                      direction, false);
    }

    bool FileSessionController::RequestFolderLoadResidency(const std::wstring& folderPath)
    {
        auto fileList = fFileList->GetSupportedFileListInFolder(folderPath);
        if (fileList.empty())
            return false;

        const auto normalizedFolderPath = std::filesystem::path(folderPath).lexically_normal().wstring();
        return StartPendingNavigation(normalizedFolderPath, std::move(fileList), 0, 1, true);
    }

    bool FileSessionController::IsCurrentFile(const std::wstring& fileName) const
    {
        return fFileList != nullptr && fFileList->IsIndexValid(fFileList->GetCurrentIndex()) &&
               fFileList->GetCurrentItemName() == fileName;
    }

    bool FileSessionController::OnFolderLoadResidencyReady(const BrowseResidencyManager::FileListSnapshot& snapshot,
                                                           const std::wstring& fileName, IMCodec::ImageSharedPtr image)
    {
        if (image == nullptr)
            return false;

        auto fileListCopy = snapshot.files;
        fFileList->SetFolder(snapshot.folderPath, std::move(fileListCopy));

        const auto previousIndex    = fFileList->GetCurrentIndex();
        const auto setCurrentResult = fFileList->SetCurrentIndexByElementName(fileName);

        if (setCurrentResult != ResultCode::RC_Success)
            return false;

        if (previousIndex == fFileList->GetCurrentIndex())
            OnFileIndexChanged(fFileList->GetCurrentIndex(), fFileList->GetCurrentIndex());

        return true;
    }

    FileSessionController::CandidateCompletionResult FileSessionController::OnCandidateResidencyReady(
        const CandidateResidencyCompletion& completion)
    {
        if (IsPendingCandidateCompletion(completion) == false)
            return {};

        if (completion.image != nullptr)
            return AcceptPendingCandidate(completion);

        return AdvancePendingCandidateAfterFailure();
    }

    void FileSessionController::RequestCurrentFileReload()
    {
        ClearPendingNavigation();
        fBrowseResidencyManager.OnCurrentFileReloadRequested(fFileList->CreateSnapshot());
    }

    void FileSessionController::OnFileIndexChanged(FileList::index_type current, FileList::index_type previous)
    {
        if (fSuppressFileIndexChanged)
            return;

        ClearPendingNavigation();
        (void) current;
        fBrowseResidencyManager.OnCurrentIndexChanged(fFileList->CreateSnapshot(), previous);
    }

    bool FileSessionController::StartPendingNavigation(std::wstring folderPath, FileList::list_string_type files,
                                                       FileList::index_type requestedIndex, int direction,
                                                       bool folderLoad)
    {
        if (files.empty() || direction == 0 || fCandidateImageReadyCallback == nullptr)
            return false;

        PendingNavigation pending;
        pending.active                 = true;
        pending.folderLoad             = folderLoad;
        pending.generation             = ++fNextPendingGeneration;
        pending.requestedIndex         = requestedIndex;
        pending.originalRequestedIndex = requestedIndex;
        pending.direction              = direction > 0 ? 1 : -1;
        pending.folderPath             = std::move(folderPath);
        pending.files                  = std::move(files);

        fPendingNavigation = std::move(pending);
        if (IsPendingIndexValid(fPendingNavigation.requestedIndex) == false)
        {
            ClearPendingNavigation();
            return false;
        }

        fPendingNavigation.requestedFile =
            fPendingNavigation.files[static_cast<std::size_t>(fPendingNavigation.requestedIndex)];
        fPendingNavigation.originalRequestedFile = fPendingNavigation.requestedFile;
        RequestPendingCandidate();
        return true;
    }

    void FileSessionController::RequestPendingCandidate()
    {
        if (fPendingNavigation.active == false || fCandidateImageReadyCallback == nullptr)
            return;

        fBrowseResidencyManager.RequestCandidateResidency(
            fPendingNavigation.requestedFile, fPendingNavigation.requestedIndex, fPendingNavigation.generation,
            [callback = fCandidateImageReadyCallback](std::uint64_t generation, std::ptrdiff_t index,
                                                      const std::wstring& fileName, IMCodec::ImageSharedPtr image)
            {
                callback(CandidateResidencyCompletion{generation, static_cast<FileList::index_type>(index), fileName,
                                                      std::move(image)});
            });
    }

    bool FileSessionController::IsPendingCandidateCompletion(const CandidateResidencyCompletion& completion) const
    {
        return fPendingNavigation.active && completion.generation == fPendingNavigation.generation &&
               completion.index == fPendingNavigation.requestedIndex &&
               completion.fileName == fPendingNavigation.requestedFile;
    }

    FileSessionController::CandidateCompletionResult FileSessionController::AcceptPendingCandidate(
        const CandidateResidencyCompletion& completion)
    {
        const auto previousIndex = fPendingNavigation.folderLoad ? FileList::IndexStart : fFileList->GetCurrentIndex();
        if (fPendingNavigation.folderLoad)
        {
            auto fileList = fPendingNavigation.files;
            fFileList->SetFolder(fPendingNavigation.folderPath, std::move(fileList));
        }

        ResultCode setCurrentResult = ResultCode::RC_Success;
        {
            const ScopedFileIndexChangeSuppression suppressFileIndexChanged(fSuppressFileIndexChanged);
            setCurrentResult = fFileList->SetCurrentIndexByElementName(completion.fileName);
        }

        if (setCurrentResult != ResultCode::RC_Success || !IsCurrentFile(completion.fileName))
        {
            ClearPendingNavigation();
            return {};
        }

        fBrowseResidencyManager.OnCurrentIndexCommitted(fFileList->CreateSnapshot(), previousIndex);
        ClearPendingNavigation();
        return {CandidateCompletionAction::LoadImage, completion.fileName, completion.image};
    }

    FileSessionController::CandidateCompletionResult FileSessionController::AdvancePendingCandidateAfterFailure()
    {
        const FileList::index_type nextIndex = fPendingNavigation.requestedIndex + fPendingNavigation.direction;
        if (IsPendingIndexValid(nextIndex))
        {
            fPendingNavigation.requestedIndex = nextIndex;
            fPendingNavigation.requestedFile  = fPendingNavigation.files[static_cast<std::size_t>(nextIndex)];
            RequestPendingCandidate();
            return {};
        }

        CandidateCompletionResult result{CandidateCompletionAction::ShowFailure,
                                         fPendingNavigation.originalRequestedFile, nullptr};
        ClearPendingNavigation();
        return result;
    }

    bool FileSessionController::IsPendingIndexValid(FileList::index_type index) const
    {
        return index >= 0 && index < static_cast<FileList::index_type>(fPendingNavigation.files.size());
    }

    void FileSessionController::ClearPendingNavigation()
    {
        if (fPendingNavigation.active)
            ++fNextPendingGeneration;

        fPendingNavigation = {};
    }
}  // namespace OIV
