#pragma once

#include <OIVAppCore/FileList.h>
#include <OIVShared/BrowseResidencyManager.h>
#include <OIVShared/ImageResidency.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace OIV
{
    class FileSessionController
    {
      public:

        using CurrentImageReadyCallback = BrowseResidencyManager::CurrentImageReadyCallback;
        using FolderLoadReadyCallback   = BrowseResidencyManager::FolderLoadReadyCallback;

        struct CandidateResidencyCompletion
        {
            std::uint64_t generation   = 0;
            FileList::index_type index = FileList::IndexStart;
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };

        enum class CandidateCompletionAction
        {
            Ignore,
            LoadImage,
            ShowFailure
        };

        struct CandidateCompletionResult
        {
            CandidateCompletionAction action = CandidateCompletionAction::Ignore;
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };

        using CandidateImageReadyCallback = std::function<void(const CandidateResidencyCompletion&)>;

        FileSessionController(IFileListProvider* fileListProvider, IFileWatcher* fileWatcher, FileSorter* fileSorter,
                              const FileList::hashset_type& knownFileTypesSet, FileList::string_type knownFileTypes,
                              ImageResidency& imageResidency, CurrentImageReadyCallback currentImageReadyCallback,
                              FolderLoadReadyCallback folderLoadReadyCallback,
                              CandidateImageReadyCallback candidateImageReadyCallback = {});

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
                                        const std::wstring& fileName, IMCodec::ImageSharedPtr image);
        CandidateCompletionResult OnCandidateResidencyReady(const CandidateResidencyCompletion& completion);
        void RequestCurrentFileReload();

      private:

        struct PendingNavigation
        {
            // Requested state is validated here; FileList's current index remains the visible decoded image until a
            // candidate succeeds.
            bool active                                 = false;
            bool folderLoad                             = false;
            // Async candidates carry this generation so completions from older requests are ignored.
            std::uint64_t generation                    = 0;
            FileList::index_type requestedIndex         = FileList::IndexStart;
            FileList::index_type originalRequestedIndex = FileList::IndexStart;
            int direction                               = 0;
            std::wstring requestedFile;
            std::wstring originalRequestedFile;
            std::wstring folderPath;
            FileList::list_string_type files;
        };

        void OnFileIndexChanged(FileList::index_type current, FileList::index_type previous);
        bool StartPendingNavigation(std::wstring folderPath, FileList::list_string_type files,
                                    FileList::index_type requestedIndex, int direction, bool folderLoad);
        void RequestPendingCandidate();
        bool IsPendingCandidateCompletion(const CandidateResidencyCompletion& completion) const;
        CandidateCompletionResult AcceptPendingCandidate(const CandidateResidencyCompletion& completion);
        CandidateCompletionResult AdvancePendingCandidateAfterFailure();
        bool IsPendingIndexValid(FileList::index_type index) const;
        void ClearPendingNavigation();

        std::unique_ptr<FileList> fFileList;
        CurrentImageReadyCallback fCurrentImageReadyCallback;
        CandidateImageReadyCallback fCandidateImageReadyCallback;
        PendingNavigation fPendingNavigation;
        std::uint64_t fNextPendingGeneration = 0;
        bool fSuppressFileIndexChanged       = false;
        BrowseResidencyManager fBrowseResidencyManager;
    };
}  // namespace OIV
