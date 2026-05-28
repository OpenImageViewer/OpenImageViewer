#include <LLUtils/StringDefs.h>
#include <OIVAppCore/BrowseSessionController.h>

#include <filesystem>
#include <utility>

namespace OIV
{
    namespace
    {
        LLUtils::native_string_type NormalizeFileIdentity(const LLUtils::native_string_type& fileName)
        {
            return std::filesystem::path(fileName).lexically_normal().native();
        }

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

    BrowseSessionController::BrowseSessionController(IFileWatcher* fileWatcher, FileSorter* fileSorter,
                                                     const FolderFileList::hashset_type& knownFileTypesSet,
                                                     FolderFileList::string_type knownFileTypes,
                                                     ImageResidencyCache& imageResidency,
                                                     CurrentImageReadyCallback currentImageReadyCallback,
                                                     CandidateImageReadyCallback candidateImageReadyCallback)
        : fFileWatcher(fileWatcher),
          fFolderFileList(std::make_unique<FolderFileList>(fileSorter, knownFileTypesSet, std::move(knownFileTypes),
                                                           [this](FolderFileList::index_type current,
                                                                  FolderFileList::index_type previous)
                                                           { OnFileIndexChanged(current, previous); })),
          fCurrentImageReadyCallback(std::move(currentImageReadyCallback)),
          fCandidateImageReadyCallback(std::move(candidateImageReadyCallback)),
          fBrowseResidencyController(imageResidency,
                                     [this](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
                                     {
                                         if (fCurrentImageReadyCallback)
                                             fCurrentImageReadyCallback(fileName, std::move(image));
                                     },
                                     {})
    {
    }

    BrowseSessionController::~BrowseSessionController()
    {
        RemoveActiveFolderWatch();
    }

    FolderFileList& BrowseSessionController::GetFolderFileList()
    {
        return *fFolderFileList;
    }

    const FolderFileList& BrowseSessionController::GetFolderFileList() const
    {
        return *fFolderFileList;
    }

    void BrowseSessionController::SortFolderFileList()
    {
        // Sorting changes navigation order, so pending candidate requests from the previous order must not commit or
        // continue scanning after they complete asynchronously.
        ClearPendingBrowseRequest();

        const auto previousIndex = fFolderFileList->GetCurrentIndex();
        fFolderFileList->Sort(fCommittedCurrentFile);
        const auto currentIndex = fFolderFileList->GetCurrentIndex();
        if (previousIndex == currentIndex && fFolderFileList->IsIndexValid(currentIndex))
            OnFileIndexChanged(currentIndex, previousIndex);
    }

    void BrowseSessionController::BeginDirectOpen(const LLUtils::native_string_type& normalizedFilePath)
    {
        ClearPendingBrowseRequest();
        const auto normalizedPath = std::filesystem::path(normalizedFilePath).lexically_normal();
        fBrowseResidencyController.SetWorkingFolder(normalizedPath.parent_path().native());
        fBrowseResidencyController.InvalidateCurrent();
    }

    ResultCode BrowseSessionController::CommitCurrentFile(const LLUtils::native_string_type& absoluteFilePath,
                                                          bool refreshResidency)
    {
        const auto normalizedPath = std::filesystem::path(absoluteFilePath).lexically_normal();
        const auto normalizedFile = normalizedPath.native();
        const auto folderPath     = normalizedPath.parent_path().native();

        if (!std::filesystem::exists(normalizedPath))
            return ResultCode::RC_FileNotFound;

        ClearPendingBrowseRequest();

        const auto previousIndex = fFolderFileList->GetCurrentIndex();
        {
            const ScopedFileIndexChangeSuppression suppressFileIndexChanged(fSuppressFileIndexChanged);
            fCommittedCurrentFile = normalizedFile;

            if (folderPath.empty() == false)
            {
                if (fFolderFileList->GetFolder() != LLUtils::FileSystemHelper::ResolveFullPath(folderPath))
                    fFolderFileList->SetFolder(folderPath, {}, normalizedFile);
                else
                    (void) fFolderFileList->SetCurrentIndexByElementName(normalizedFile);
            }
            else
            {
                fFolderFileList->InvalidateCurrentIndex();
            }

            if (HasCommittedFolderRepresentation() == false)
                fFolderFileList->InvalidateCurrentIndex();
        }

        if (HasCommittedFolderRepresentation())
            WatchFolder(folderPath);
        else
            RemoveActiveFolderWatch();

        if (refreshResidency && HasCommittedFolderRepresentation())
            fBrowseResidencyController.RefreshCommittedCurrent(fFolderFileList->CreateSnapshot());

        return ResultCode::RC_Success;
    }

    void BrowseSessionController::InvalidateCurrent()
    {
        ClearPendingBrowseRequest();
        fBrowseResidencyController.InvalidateCurrent();
    }

    bool BrowseSessionController::JumpFiles(FolderFileList::index_type step)
    {
        if (fFolderFileList->GetSize() == 0 ||
            fFolderFileList->IsIndexValid(fFolderFileList->GetCurrentIndex()) == false)
            return false;

        const FolderFileList::index_type currentIndex = fFolderFileList->GetCurrentIndex();
        FolderFileList::index_type requestedIndex     = step;
        int direction                                 = 1;
        if (fFolderFileList->IsMarkerIndex(step))
        {
            requestedIndex = fFolderFileList->GetIndexFromMarker(step);
            direction      = requestedIndex >= currentIndex ? 1 : -1;
        }
        else
        {
            requestedIndex = currentIndex + step;
            direction      = step >= 0 ? 1 : -1;
        }

        return StartPendingBrowseRequest(fFolderFileList->GetFolder(), fFolderFileList->CreateSnapshot().files,
                                         requestedIndex, direction, false);
    }

    bool BrowseSessionController::RequestFolderLoadResidency(const LLUtils::native_string_type& folderPath)
    {
        auto fileList = fFolderFileList->GetSupportedFolderFileListInFolder(folderPath);
        if (fileList.empty())
            return false;

        const auto normalizedFolderPath = std::filesystem::path(folderPath).lexically_normal().native();
        return StartPendingBrowseRequest(normalizedFolderPath, std::move(fileList), 0, 1, true);
    }

    bool BrowseSessionController::IsCurrentFile(const LLUtils::native_string_type& fileName) const
    {
        return NormalizeFileIdentity(fileName) == fCommittedCurrentFile;
    }

    const LLUtils::native_string_type& BrowseSessionController::GetCommittedCurrentFile() const
    {
        return fCommittedCurrentFile;
    }

    IFileWatcher::FolderID BrowseSessionController::GetActiveFolderID() const
    {
        return fActiveFolderID;
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::OnFileChanged(
        const IFileWatcher::FileChangedEventArgs& fileChangedEventArgs)
    {
        if (fActiveFolderID == IFileWatcher::FolderID{} || fileChangedEventArgs.folderID != fActiveFolderID)
            return {};

        if (fileChangedEventArgs.fileOp != IFileWatcher::FileChangedOp::Modified)
            ClearPendingBrowseRequest();

        const auto absPath = NormalizeFileIdentity(
            (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName).native());
        const auto absPath2 = NormalizeFileIdentity(
            (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName2).native());

        if (fileChangedEventArgs.fileOp == IFileWatcher::FileChangedOp::WatchedFolderRemoved)
        {
            fFolderFileList->InvalidateCurrentIndex();
            fActiveFolderID = {};
            fActiveWatchedFolder.clear();
            return {};
        }

        if (fileChangedEventArgs.fileOp == IFileWatcher::FileChangedOp::Remove && absPath == fCommittedCurrentFile)
        {
            fFolderFileList->UpdateFolderFileList(fileChangedEventArgs.fileOp, absPath, absPath2,
                                                  fCommittedCurrentFile);
            fCommittedCurrentFile.clear();
            fBrowseResidencyController.InvalidateCurrent();
            return {BrowseSessionAction::CurrentFileRemoved, absPath, nullptr};
        }

        if (fileChangedEventArgs.fileOp == IFileWatcher::FileChangedOp::Rename && absPath == fCommittedCurrentFile)
        {
            if (fFolderFileList->IsSupportedFileType(absPath2) == false)
            {
                fFolderFileList->UpdateFolderFileList(IFileWatcher::FileChangedOp::Remove, absPath, {},
                                                      fCommittedCurrentFile);
                fCommittedCurrentFile.clear();
                fBrowseResidencyController.InvalidateCurrent();
                return {BrowseSessionAction::CurrentFileUnsupportedRename, absPath, nullptr};
            }

            {
                const ScopedFileIndexChangeSuppression suppressFileIndexChanged(fSuppressFileIndexChanged);
                fCommittedCurrentFile = absPath2;
                fFolderFileList->UpdateFolderFileList(fileChangedEventArgs.fileOp, absPath, absPath2,
                                                      fCommittedCurrentFile);
            }

            if (HasCommittedFolderRepresentation())
                fBrowseResidencyController.ReloadCurrent(fFolderFileList->CreateSnapshot());

            return {};
        }

        fFolderFileList->UpdateFolderFileList(fileChangedEventArgs.fileOp, absPath, absPath2, fCommittedCurrentFile);
        if (HasCommittedFolderRepresentation())
            fBrowseResidencyController.RefreshCommittedCurrent(fFolderFileList->CreateSnapshot());

        return {};
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::OnFolderOpenCandidateReady(
        const BrowseCandidateCompletion& completion)
    {
        return OnPendingCandidateReady(completion, true);
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::OnBrowseCandidateReady(
        const BrowseCandidateCompletion& completion)
    {
        return OnPendingCandidateReady(completion, false);
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::OnPendingCandidateReady(
        const BrowseCandidateCompletion& completion, bool expectedFolderLoad)
    {
        if (IsPendingCandidateCompletion(completion) == false)
            return {};

        if (fPendingBrowseRequest.folderLoad != expectedFolderLoad)
            return {};

        if (completion.image != nullptr)
            return AcceptPendingCandidate(completion);

        return AdvancePendingCandidateAfterFailure();
    }

    void BrowseSessionController::RequestCurrentFileReload()
    {
        ClearPendingBrowseRequest();
        fBrowseResidencyController.ReloadCurrent(fFolderFileList->CreateSnapshot());
    }

    void BrowseSessionController::OnFileIndexChanged(FolderFileList::index_type current,
                                                     FolderFileList::index_type previous)
    {
        if (fSuppressFileIndexChanged)
            return;

        ClearPendingBrowseRequest();
        (void) current;
        fBrowseResidencyController.RefreshCommittedCurrent(fFolderFileList->CreateSnapshot());
    }

    void BrowseSessionController::WatchFolder(const LLUtils::native_string_type& folderPath)
    {
        if (fFileWatcher == nullptr)
            return;

        const auto normalizedFolderPath = LLUtils::FileSystemHelper::ResolveFullPath(folderPath);
        if (fActiveWatchedFolder == normalizedFolderPath)
            return;

        RemoveActiveFolderWatch();
        fActiveWatchedFolder = normalizedFolderPath;
        fActiveFolderID      = fFileWatcher->AddFolder(fActiveWatchedFolder);
    }

    void BrowseSessionController::RemoveActiveFolderWatch()
    {
        if (fFileWatcher != nullptr && fActiveFolderID != IFileWatcher::FolderID{})
            fFileWatcher->RemoveFolder(fActiveFolderID);

        fActiveFolderID = {};
        fActiveWatchedFolder.clear();
    }

    bool BrowseSessionController::StartPendingBrowseRequest(LLUtils::native_string_type folderPath,
                                                            FolderFileList::list_string_type files,
                                                            FolderFileList::index_type requestedIndex, int direction,
                                                            bool folderLoad)
    {
        if (files.empty() || direction == 0 || fCandidateImageReadyCallback == nullptr)
            return false;

        PendingBrowseRequest pending;
        pending.active                 = true;
        pending.folderLoad             = folderLoad;
        pending.generation             = ++fNextPendingGeneration;
        pending.requestedIndex         = requestedIndex;
        pending.originalRequestedIndex = requestedIndex;
        pending.direction              = direction > 0 ? 1 : -1;
        pending.folderPath             = std::move(folderPath);
        pending.files                  = std::move(files);

        fPendingBrowseRequest = std::move(pending);
        if (IsPendingIndexValid(fPendingBrowseRequest.requestedIndex) == false)
        {
            ClearPendingBrowseRequest();
            return false;
        }

        fPendingBrowseRequest.requestedFile = NormalizeFileIdentity(
            fPendingBrowseRequest.files[static_cast<std::size_t>(fPendingBrowseRequest.requestedIndex)]);
        fPendingBrowseRequest.originalRequestedFile = fPendingBrowseRequest.requestedFile;
        RequestPendingCandidate();
        return true;
    }

    void BrowseSessionController::RequestPendingCandidate()
    {
        if (fPendingBrowseRequest.active == false || fCandidateImageReadyCallback == nullptr)
            return;

        fBrowseResidencyController.RequestCandidateResidency(
            fPendingBrowseRequest.requestedFile, fPendingBrowseRequest.requestedIndex, fPendingBrowseRequest.generation,
            [callback = fCandidateImageReadyCallback, folderLoad = fPendingBrowseRequest.folderLoad](
                std::uint64_t generation, std::ptrdiff_t index, const LLUtils::native_string_type& fileName,
                IMCodec::ImageSharedPtr image)
            {
                callback(BrowseCandidateCompletion{generation, static_cast<FolderFileList::index_type>(index), fileName,
                                                   std::move(image), folderLoad});
            });
    }

    bool BrowseSessionController::IsPendingCandidateCompletion(const BrowseCandidateCompletion& completion) const
    {
        return fPendingBrowseRequest.active && completion.generation == fPendingBrowseRequest.generation &&
               completion.index == fPendingBrowseRequest.requestedIndex &&
               NormalizeFileIdentity(completion.fileName) == fPendingBrowseRequest.requestedFile &&
               completion.folderLoad == fPendingBrowseRequest.folderLoad;
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::AcceptPendingCandidate(
        const BrowseCandidateCompletion& completion)
    {
        const auto acceptedFile  = fPendingBrowseRequest.requestedFile;
        const auto previousIndex = fPendingBrowseRequest.folderLoad ? FolderFileList::IndexStart
                                                                    : fFolderFileList->GetCurrentIndex();
        if (fPendingBrowseRequest.folderLoad)
        {
            auto fileList = fPendingBrowseRequest.files;
            fFolderFileList->SetFolder(fPendingBrowseRequest.folderPath, std::move(fileList), acceptedFile);
        }

        ResultCode setCurrentResult = ResultCode::RC_Success;
        {
            const ScopedFileIndexChangeSuppression suppressFileIndexChanged(fSuppressFileIndexChanged);
            setCurrentResult      = fFolderFileList->SetCurrentIndexByElementName(acceptedFile);
            fCommittedCurrentFile = acceptedFile;
        }

        if (setCurrentResult != ResultCode::RC_Success || HasCommittedFolderRepresentation() == false)
        {
            ClearPendingBrowseRequest();
            return {};
        }

        WatchFolder(fFolderFileList->GetFolder());
        fBrowseResidencyController.CommitNavigation(fFolderFileList->CreateSnapshot(), previousIndex);
        ClearPendingBrowseRequest();
        return {BrowseSessionAction::DisplayImage, acceptedFile, completion.image};
    }

    BrowseSessionController::BrowseSessionResult BrowseSessionController::AdvancePendingCandidateAfterFailure()
    {
        const FolderFileList::index_type nextIndex = fPendingBrowseRequest.requestedIndex +
                                                     fPendingBrowseRequest.direction;
        if (IsPendingIndexValid(nextIndex))
        {
            fPendingBrowseRequest.requestedIndex = nextIndex;
            fPendingBrowseRequest.requestedFile  = NormalizeFileIdentity(
                fPendingBrowseRequest.files[static_cast<std::size_t>(nextIndex)]);
            RequestPendingCandidate();
            return {};
        }

        BrowseSessionResult result{BrowseSessionAction::ShowFailure, fPendingBrowseRequest.originalRequestedFile,
                                   nullptr};
        ClearPendingBrowseRequest();
        return result;
    }

    bool BrowseSessionController::IsPendingIndexValid(FolderFileList::index_type index) const
    {
        return index >= 0 && index < static_cast<FolderFileList::index_type>(fPendingBrowseRequest.files.size());
    }

    bool BrowseSessionController::HasCommittedFolderRepresentation() const
    {
        return fFolderFileList->IsIndexValid(fFolderFileList->GetCurrentIndex()) &&
               fFolderFileList->GetCurrentItemName() == fCommittedCurrentFile;
    }

    void BrowseSessionController::ClearPendingBrowseRequest()
    {
        if (fPendingBrowseRequest.active)
            ++fNextPendingGeneration;

        fPendingBrowseRequest = {};
    }
}  // namespace OIV
