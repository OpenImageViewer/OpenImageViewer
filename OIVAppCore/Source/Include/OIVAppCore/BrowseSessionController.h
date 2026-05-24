#pragma once

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
            std::wstring fileName;
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
            std::wstring fileName;
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
        void BeginDirectOpen(const std::wstring& normalizedFilePath);
        ResultCode CommitCurrentFile(const std::wstring& absoluteFilePath, bool refreshResidency = true);
        void InvalidateCurrent();
        bool JumpFiles(FolderFileList::index_type step);
        bool RequestFolderLoadResidency(const std::wstring& folderPath);
        bool IsCurrentFile(const std::wstring& fileName) const;
        const std::wstring& GetCommittedCurrentFile() const;
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
            std::wstring requestedFile;
            std::wstring originalRequestedFile;
            std::wstring folderPath;
            FolderFileList::list_string_type files;
        };

        void OnFileIndexChanged(FolderFileList::index_type current, FolderFileList::index_type previous);
        void WatchFolder(const std::wstring& folderPath);
        void RemoveActiveFolderWatch();
        bool StartPendingBrowseRequest(std::wstring folderPath, FolderFileList::list_string_type files,
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
        std::wstring fActiveWatchedFolder;
        std::wstring fCommittedCurrentFile;
        std::unique_ptr<FolderFileList> fFolderFileList;
        CurrentImageReadyCallback fCurrentImageReadyCallback;
        CandidateImageReadyCallback fCandidateImageReadyCallback;
        PendingBrowseRequest fPendingBrowseRequest;
        std::uint64_t fNextPendingGeneration = 0;
        bool fSuppressFileIndexChanged       = false;
        BrowseResidencyController fBrowseResidencyController;
    };
}  // namespace OIV
