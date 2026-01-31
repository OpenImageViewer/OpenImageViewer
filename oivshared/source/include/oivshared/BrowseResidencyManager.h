#pragma once

#include <Image.h>
#include <LLUtils/StringDefs.h>
#include <oivshared/ImageResidency.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace OIV
{
    class BrowseResidencyManager
    {
      public:

        struct FileListSnapshot
        {
            using string_type = LLUtils::native_string_type;
            using list_string_type = LLUtils::ListString<string_type>;
            using index_type = list_string_type::difference_type;

            static constexpr index_type IndexEnd = std::numeric_limits<index_type>::max();
            static constexpr index_type IndexStart = std::numeric_limits<index_type>::min();

            FileListSnapshot() = default;

            FileListSnapshot(string_type folderPathParam, list_string_type filesParam, index_type currentIndexParam)
                : folderPath(std::move(folderPathParam))
                , files(std::move(filesParam))
                , currentIndex(currentIndexParam)
            {
            }

            template <typename Snapshot>
            FileListSnapshot(const Snapshot& snapshot)
                : folderPath(snapshot.folderPath)
                , files(snapshot.files.begin(), snapshot.files.end())
                , currentIndex(static_cast<index_type>(snapshot.currentIndex))
            {
            }

            string_type folderPath;
            list_string_type files;
            index_type currentIndex = IndexStart;
        };
        using CurrentImageReadyCallback =
            std::function<void(const LLUtils::native_string_type&, IMCodec::ImageSharedPtr)>;
        using FolderLoadReadyCallback = std::function<void(const FileListSnapshot&,
                                                           const LLUtils::native_string_type&,
                                                           IMCodec::ImageSharedPtr)>;

        static constexpr std::size_t PredictedImageCount = 2;
        static constexpr std::size_t ResidentImageCount  = 5;

        BrowseResidencyManager(ImageResidency& imageResidency,
                               CurrentImageReadyCallback currentImageReadyCallback,
                               FolderLoadReadyCallback folderLoadReadyCallback)
            : fImageResidency(imageResidency)
            , fCurrentImageReadyCallback(std::move(currentImageReadyCallback))
            , fFolderLoadReadyCallback(std::move(folderLoadReadyCallback))
        {
        }

        ~BrowseResidencyManager()
        {
            WaitForPendingTasks();
        }

        void OnCurrentIndexChanged(const FileListSnapshot& snapshot, std::ptrdiff_t previous)
        {
            SetWorkingFolder(snapshot.folderPath);
            CleanupCompletedTasks();
            ApplyPolicy(snapshot, ResolveDirection(snapshot.currentIndex, previous), false);
        }

        void OnCurrentFileReloadRequested(const FileListSnapshot& snapshot)
        {
            SetWorkingFolder(snapshot.folderPath);
            CleanupCompletedTasks();
            ApplyPolicy(snapshot, 0, true);
        }

        void RequestFolderLoadResidency(const FileListSnapshot& snapshot)
        {
            SetWorkingFolder(snapshot.folderPath);
            CleanupCompletedTasks();

            std::uint64_t generation = 0;
            {
                std::lock_guard lock(fMutex);
                ++fGeneration;
                generation = fGeneration;
            }

            fPendingTasks.push_back(RequestFolderLoadResidencyAsync(snapshot, generation));
        }

        void SetWorkingFolder(const LLUtils::native_string_type& folderPath)
        {
            CleanupCompletedTasks();

            std::vector<LLUtils::native_string_type> filesToEvict;
            bool shouldEvict = false;
            {
                std::lock_guard lock(fMutex);
                if (fWorkingFolder == folderPath)
                    return;

                filesToEvict.assign(fTrackedFiles.begin(), fTrackedFiles.end());
                fTrackedFiles.clear();
                fDesiredFiles.clear();
                fRecentFiles.clear();
                fCurrentSnapshot = FileListSnapshot{};
                fDirection    = 1;
                fWorkingFolder = folderPath;
                ++fGeneration;
                shouldEvict = true;
            }

            if (shouldEvict)
                EvictFiles(filesToEvict);
        }

        void InvalidateCurrent()
        {
            CleanupCompletedTasks();

            std::lock_guard lock(fMutex);
            fCurrentSnapshot = FileListSnapshot{};
            ++fGeneration;
        }

        void Reset()
        {
            CleanupCompletedTasks();

            std::vector<LLUtils::native_string_type> filesToEvict;
            {
                std::lock_guard lock(fMutex);
                filesToEvict.assign(fTrackedFiles.begin(), fTrackedFiles.end());
                fTrackedFiles.clear();
                fDesiredFiles.clear();
                fRecentFiles.clear();
                fCurrentSnapshot = FileListSnapshot{};
                fDirection    = 1;
                fWorkingFolder.clear();
                ++fGeneration;
            }

            EvictFiles(filesToEvict);
        }

        void CleanupCompletedTasks()
        {
            std::erase_if(fPendingTasks, [](const TicketID& task) { return task.await_ready(); });
        }

      private:

        struct PolicyUpdate
        {
            std::vector<LLUtils::native_string_type> filesToRequest;
            std::vector<LLUtils::native_string_type> filesToEvict;
            LLUtils::native_string_type currentFile;
            std::uint64_t generation = 0;
        };

        static constexpr std::ptrdiff_t InvalidIndex = static_cast<std::ptrdiff_t>(-1);

        static bool IsIndexValid(const FileListSnapshot& snapshot, std::ptrdiff_t index)
        {
            return index >= 0 && index < static_cast<std::ptrdiff_t>(snapshot.files.size());
        }

        static int ResolveDirection(std::ptrdiff_t current, std::ptrdiff_t previous)
        {
            if (previous == InvalidIndex || current == previous)
                return 1;

            return current > previous ? 1 : -1;
        }

        static void AppendUnique(std::vector<LLUtils::native_string_type>& orderedFiles,
                                 std::set<LLUtils::native_string_type>& desiredFiles,
                                 const LLUtils::native_string_type& fileName)
        {
            if (desiredFiles.insert(fileName).second)
                orderedFiles.push_back(fileName);
        }

        bool IsFolderLoadCurrentLocked(const FileListSnapshot& snapshot, std::uint64_t generation) const
        {
            return generation == fGeneration && snapshot.folderPath == fWorkingFolder;
        }

        void ApplyPolicy(const FileListSnapshot& snapshot, int requestedDirection, bool forceReloadCurrent)
        {
            PolicyUpdate update{};

            {
                std::lock_guard lock(fMutex);
                ++fGeneration;
                fCurrentSnapshot = snapshot;
                if (IsIndexValid(fCurrentSnapshot, fCurrentSnapshot.currentIndex) == false)
                    fCurrentSnapshot.currentIndex = InvalidIndex;

                if (requestedDirection != 0)
                    fDirection = requestedDirection;

                if (fDirection == 0)
                    fDirection = 1;

                if (fCurrentSnapshot.currentIndex != InvalidIndex)
                    TouchRecentFileLocked(fCurrentSnapshot.files[static_cast<std::size_t>(fCurrentSnapshot.currentIndex)]);

                update = BuildPolicyUpdateLocked();
            }

            if (forceReloadCurrent && update.currentFile.empty() == false)
                fImageResidency.removeResidency(update.currentFile, ImageResidencyItemType::FullSize);

            EvictFiles(update.filesToEvict);
            RequestFiles(update.filesToRequest, update.currentFile, update.generation);
        }

        PolicyUpdate BuildPolicyUpdateLocked()
        {
            PolicyUpdate update{};
            update.generation = fGeneration;

            std::vector<LLUtils::native_string_type> orderedFiles;
            std::set<LLUtils::native_string_type> desiredFiles;
            if (IsIndexValid(fCurrentSnapshot, fCurrentSnapshot.currentIndex))
            {
                update.currentFile = fCurrentSnapshot.files[static_cast<std::size_t>(fCurrentSnapshot.currentIndex)];
                AppendUnique(orderedFiles, desiredFiles, update.currentFile);

                for (std::size_t offset = 1; offset <= PredictedImageCount; ++offset)
                {
                    const auto signedOffset = static_cast<std::ptrdiff_t>(offset);
                    const std::ptrdiff_t predictedIndex =
                        fCurrentSnapshot.currentIndex + (fDirection > 0 ? signedOffset : -signedOffset);
                    if (!IsIndexValid(fCurrentSnapshot, predictedIndex))
                        break;

                    AppendUnique(orderedFiles, desiredFiles,
                                 fCurrentSnapshot.files[static_cast<std::size_t>(predictedIndex)]);
                }

                for (const auto& fileName : fRecentFiles)
                {
                    if (desiredFiles.size() >= ResidentImageCount)
                        break;

                    AppendUnique(orderedFiles, desiredFiles, fileName);
                }
            }

            for (const auto& fileName : fTrackedFiles)
            {
                if (!desiredFiles.contains(fileName))
                    update.filesToEvict.push_back(fileName);
            }

            update.filesToRequest = std::move(orderedFiles);
            fTrackedFiles = desiredFiles;
            fDesiredFiles = desiredFiles;
            return update;
        }

        void TouchRecentFileLocked(const LLUtils::native_string_type& fileName)
        {
            std::erase(fRecentFiles, fileName);
            fRecentFiles.push_front(fileName);
            if (fRecentFiles.size() > ResidentImageCount)
                fRecentFiles.resize(ResidentImageCount);
        }

        void EvictFiles(const std::vector<LLUtils::native_string_type>& filesToEvict)
        {
            for (const auto& fileName : filesToEvict)
                fImageResidency.removeResidency(fileName, ImageResidencyItemType::FullSize);
        }

        void RequestFiles(const std::vector<LLUtils::native_string_type>& filesToRequest,
                          const LLUtils::native_string_type& currentFile,
                          std::uint64_t generation)
        {
            for (const auto& fileName : filesToRequest)
                fPendingTasks.push_back(RequestManagedResidencyAsync(fileName, generation, fileName == currentFile));
        }

        void WaitForPendingTasks()
        {
            std::vector<TicketID> pendingTasks = std::move(fPendingTasks);
            for (auto& task : pendingTasks)
            {
                try
                {
                    (void) task.get();
                }
                catch (TaskError)
                {
                }
            }
        }

        TicketID RequestManagedResidencyAsync(LLUtils::native_string_type fileName,
                                              std::uint64_t generation,
                                              bool isCurrentRequest)
        {
            try
            {
                IMCodec::ImageSharedPtr image =
                    co_await fImageResidency.requestResidencyAsync(fileName, ImageResidencyItemType::FullSize);

                bool shouldDeliverCurrent = false;
                bool shouldEvict          = image == nullptr;
                {
                    std::lock_guard lock(fMutex);
                    shouldDeliverCurrent = isCurrentRequest && generation == fGeneration &&
                                           IsIndexValid(fCurrentSnapshot, fCurrentSnapshot.currentIndex) &&
                                           fCurrentSnapshot.files[static_cast<std::size_t>(fCurrentSnapshot.currentIndex)] == fileName;
                    shouldEvict = shouldEvict || !fDesiredFiles.contains(fileName);
                }

                if (shouldEvict)
                    fImageResidency.removeResidency(fileName, ImageResidencyItemType::FullSize);

                if (shouldDeliverCurrent && image != nullptr && fCurrentImageReadyCallback)
                    fCurrentImageReadyCallback(fileName, image);

                co_return image;
            }
            catch (TaskError)
            {
                co_return IMCodec::ImageSharedPtr{};
            }
        }

        TicketID RequestFolderLoadResidencyAsync(FileListSnapshot snapshot, std::uint64_t generation)
        {
            try
            {
                for (const auto& fileName : snapshot.files)
                {
                    IMCodec::ImageSharedPtr image =
                        co_await fImageResidency.requestResidencyAsync(fileName, ImageResidencyItemType::FullSize);

                    {
                        std::lock_guard lock(fMutex);
                        if (IsFolderLoadCurrentLocked(snapshot, generation) == false)
                            co_return IMCodec::ImageSharedPtr{};
                    }

                    if (image != nullptr)
                    {
                        if (fFolderLoadReadyCallback)
                            fFolderLoadReadyCallback(snapshot, fileName, image);

                        co_return image;
                    }
                }

                co_return IMCodec::ImageSharedPtr{};
            }
            catch (TaskError)
            {
                co_return IMCodec::ImageSharedPtr{};
            }
        }

      private:

        ImageResidency& fImageResidency;
        CurrentImageReadyCallback fCurrentImageReadyCallback;
        FolderLoadReadyCallback fFolderLoadReadyCallback;
        std::vector<TicketID> fPendingTasks;
        std::mutex fMutex;
        FileListSnapshot fCurrentSnapshot;
        std::set<LLUtils::native_string_type> fTrackedFiles;
        std::set<LLUtils::native_string_type> fDesiredFiles;
        std::deque<LLUtils::native_string_type> fRecentFiles;
        LLUtils::native_string_type fWorkingFolder;
        int fDirection               = 1;
        std::uint64_t fGeneration    = 0;
    };
}  // namespace OIV
