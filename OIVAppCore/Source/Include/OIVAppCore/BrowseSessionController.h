#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/FolderFileList.h>
#include <OIVShared/BrowseResidencyController.h>
#include <OIVShared/ImageResidencyCache.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace OIV
{
    class BrowseSessionController
    {
      public:

        using CurrentImageReadyCallback = BrowseResidencyController::CurrentImageReadyCallback;

        struct BrowseCandidateCompletion
        {
            std::uint64_t generation         = 0;
            FolderFileList::index_type index = FolderFileList::IndexStart;
            LLUtils::native_string_type fileName;
            IMCodec::ImageSharedPtr image;
            bool folderLoad = false;
        };

        enum class BrowseSessionAction
        {
            Ignore,
            DisplayImage,
            ShowFailure,
            CurrentFileRemoved,
            CurrentFileUnsupportedRename
        };

        struct BrowseSessionResult
        {
            BrowseSessionAction action = BrowseSessionAction::Ignore;
            LLUtils::native_string_type fileName;
            IMCodec::ImageSharedPtr image;
        };

        using CandidateImageReadyCallback = std::function<void(const BrowseCandidateCompletion&)>;

        BrowseSessionController(IFileWatcher* fileWatcher, FileSorter* fileSorter,
                                const FolderFileList::hashset_type& knownFileTypesSet,
                                FolderFileList::string_type knownFileTypes, ImageResidencyCache& imageResidency,
                                CurrentImageReadyCallback currentImageReadyCallback,
                                CandidateImageReadyCallback candidateImageReadyCallback = {});
        ~BrowseSessionController();

        FolderFileList& GetFolderFileList();
        const FolderFileList& GetFolderFileList() const;

        void SortFolderFileList();
        void BeginDirectOpen(const LLUtils::native_string_type& normalizedFilePath);
        ResultCode CommitCurrentFile(const LLUtils::native_string_type& absoluteFilePath, bool refreshResidency = true);
        void InvalidateCurrent();
        bool JumpFiles(FolderFileList::index_type step);
        bool RequestFolderLoadResidency(const LLUtils::native_string_type& folderPath);
        bool IsCurrentFile(const LLUtils::native_string_type& fileName) const;
        const LLUtils::native_string_type& GetCommittedCurrentFile() const;
        IFileWatcher::FolderID GetActiveFolderID() const;
        BrowseSessionResult OnFileChanged(const IFileWatcher::FileChangedEventArgs& fileChangedEventArgs);
        BrowseSessionResult OnFolderOpenCandidateReady(const BrowseCandidateCompletion& completion);
        BrowseSessionResult OnBrowseCandidateReady(const BrowseCandidateCompletion& completion);
        void RequestCurrentFileReload();

      private:

        struct PendingBrowseRequest
        {
            // Requested state is validated here; FolderFileList's current index remains the visible decoded image until
            // a candidate succeeds.
            bool active     = false;
            bool folderLoad = false;
            // Async candidates carry this generation so completions from older requests are ignored.
            std::uint64_t generation                          = 0;
            FolderFileList::index_type requestedIndex         = FolderFileList::IndexStart;
            FolderFileList::index_type originalRequestedIndex = FolderFileList::IndexStart;
            int direction                                     = 0;
            LLUtils::native_string_type requestedFile;
            LLUtils::native_string_type originalRequestedFile;
            LLUtils::native_string_type folderPath;
            FolderFileList::list_string_type files;
        };

        void OnFileIndexChanged(FolderFileList::index_type current, FolderFileList::index_type previous);
        void WatchFolder(const LLUtils::native_string_type& folderPath);
        void RemoveActiveFolderWatch();
        bool StartPendingBrowseRequest(LLUtils::native_string_type folderPath, FolderFileList::list_string_type files,
                                       FolderFileList::index_type requestedIndex, int direction, bool folderLoad);
        void RequestPendingCandidate();
        bool IsPendingCandidateCompletion(const BrowseCandidateCompletion& completion) const;
        BrowseSessionResult OnPendingCandidateReady(const BrowseCandidateCompletion& completion,
                                                    bool expectedFolderLoad);
        BrowseSessionResult AcceptPendingCandidate(const BrowseCandidateCompletion& completion);
        BrowseSessionResult AdvancePendingCandidateAfterFailure();
        bool IsPendingIndexValid(FolderFileList::index_type index) const;
        bool HasCommittedFolderRepresentation() const;
        void ClearPendingBrowseRequest();

        IFileWatcher* fFileWatcher{};
        IFileWatcher::FolderID fActiveFolderID{};
        LLUtils::native_string_type fActiveWatchedFolder;
        LLUtils::native_string_type fCommittedCurrentFile;
        std::unique_ptr<FolderFileList> fFolderFileList;
        CurrentImageReadyCallback fCurrentImageReadyCallback;
        CandidateImageReadyCallback fCandidateImageReadyCallback;
        PendingBrowseRequest fPendingBrowseRequest;
        std::uint64_t fNextPendingGeneration = 0;
        bool fSuppressFileIndexChanged       = false;
        BrowseResidencyController fBrowseResidencyController;
    };
}  // namespace OIV
