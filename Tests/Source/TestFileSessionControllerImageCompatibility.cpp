#include "ImageMagickTestCorpus.h"

#include <catch2/catch_all.hpp>

#include <OIVAppCore/FileSessionController.h>
#include <OIVAppCore/IFileWatcher.h>
#include <OIVShared/FileSorter.h>
#include <OIVShared/ImageResidency.h>

#include <ImageLoader.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <vector>

namespace
{
    class FakeFileWatcher : public OIV::IFileWatcher
    {
      public:

        bool IsFolderRegistered(const std::wstring& folder) const override { return fFolder == folder; }

        FolderID AddFolder(const std::wstring& folder) override
        {
            fFolder = folder;
            return fFolderID;
        }

        void RemoveFolder(FolderID folderID) override
        {
            if (folderID == fFolderID)
                fFolder.clear();
        }

        void RemoveFolder(const std::wstring& folder) override
        {
            if (folder == fFolder)
                fFolder.clear();
        }

        OnFileChangedEventArgsEvent& GetFileChangedEvent() override { return fEvent; }

      private:

        FolderID fFolderID = 11;
        std::wstring fFolder;
        OnFileChangedEventArgsEvent fEvent;
    };

    class ActiveFileProvider : public OIV::IFileListProvider
    {
      public:

        OIV::FileListStringType GetActiveFileName() override
        {
            std::lock_guard lock(fMutex);
            return fActiveFile;
        }

        void SetActiveFileName(std::wstring fileName)
        {
            std::lock_guard lock(fMutex);
            fActiveFile = std::move(fileName);
        }

      private:

        std::mutex fMutex;
        std::wstring fActiveFile;
    };

    struct BrowseObserver
    {
        struct LoadedEvent
        {
            std::wstring fileName;
            bool hasImage = false;
        };

        void MarkLoaded(const std::wstring& fileName, bool hasImage = true)
        {
            {
                std::lock_guard lock(mutex);
                loadedFiles.push_back(LoadedEvent{fileName, hasImage});
                lastLoadedFile = fileName;
            }
            cv.notify_all();
        }

        bool WaitForLoaded(const std::wstring& fileName, std::chrono::milliseconds timeout = std::chrono::seconds(60))
        {
            std::unique_lock lock(mutex);
            return cv.wait_for(lock, timeout,
                               [&]
                               {
                                   return std::ranges::any_of(loadedFiles, [&](const LoadedEvent& event)
                                                              { return event.fileName == fileName && event.hasImage; });
                               });
        }

        bool WaitForLoadedCount(std::size_t count, std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(mutex);
            return cv.wait_for(lock, timeout, [&] { return loadedFiles.size() >= count; });
        }

        bool WaitForNoLoaded(std::chrono::milliseconds timeout)
        {
            return !WaitForLoadedCount(loadedFiles.size() + 1, timeout);
        }

        std::size_t CountLoaded(const std::wstring& fileName)
        {
            std::lock_guard lock(mutex);
            return static_cast<std::size_t>(std::ranges::count_if(
                loadedFiles, [&](const LoadedEvent& event) { return event.fileName == fileName && event.hasImage; }));
        }

        void Clear()
        {
            std::lock_guard lock(mutex);
            loadedFiles.clear();
            lastLoadedFile.clear();
        }

        std::vector<LoadedEvent> Snapshot()
        {
            std::lock_guard lock(mutex);
            return loadedFiles;
        }

        std::mutex mutex;
        std::condition_variable cv;
        std::vector<LoadedEvent> loadedFiles;
        std::wstring lastLoadedFile;
    };

    class ControlledImageMagickResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        bool ProcessResidencyRequest(const OIV::ImageResidencyKey& key, OIV::ImageResidencyValue& outValue) override
        {
            const auto fileName = std::get<0>(key);
            MarkStarted(fileName);
            WaitUntilReleased(fileName);

            try
            {
                IMCodec::ImageLoader loader;
                const auto result = loader.Decode(fileName, IMCodec::ImageLoadFlags::None, {},
                                                  IMCodec::PluginTraverseMode::AnyFileType |
                                                      IMCodec::PluginTraverseMode::AnyPlugin,
                                                  outValue);
                MarkCompleted(fileName);
                return result == IMCodec::ImageResult::Success && outValue != nullptr;
            }
            catch (...)
            {
                MarkCompleted(fileName);
                outValue = nullptr;
                return false;
            }
        }

        bool WaitForStarted(const std::wstring& fileName, std::chrono::milliseconds timeout = std::chrono::seconds(30))
        {
            std::unique_lock lock(fMutex);
            return fCv.wait_for(lock, timeout, [&] { return fStartedCounts[fileName] > 0; });
        }

        bool WaitForCompleted(const std::wstring& fileName,
                              std::chrono::milliseconds timeout = std::chrono::seconds(30))
        {
            std::unique_lock lock(fMutex);
            return fCv.wait_for(lock, timeout, [&] { return fCompletedCounts[fileName] > 0; });
        }

        void Release(const std::wstring& fileName)
        {
            {
                std::lock_guard lock(fMutex);
                fReleasedFiles.insert(fileName);
            }
            fCv.notify_all();
        }

        void ReleaseAll()
        {
            {
                std::lock_guard lock(fMutex);
                fReleaseEverything = true;
            }
            fCv.notify_all();
        }

      private:

        void MarkStarted(const std::wstring& fileName)
        {
            {
                std::lock_guard lock(fMutex);
                ++fStartedCounts[fileName];
                fStartedFiles.push_back(fileName);
            }
            fCv.notify_all();
        }

        void MarkCompleted(const std::wstring& fileName)
        {
            {
                std::lock_guard lock(fMutex);
                ++fCompletedCounts[fileName];
            }
            fCv.notify_all();
        }

        void WaitUntilReleased(const std::wstring& fileName)
        {
            std::unique_lock lock(fMutex);
            fCv.wait_for(lock, std::chrono::seconds(30),
                         [&] { return fReleaseEverything || fReleasedFiles.contains(fileName); });
        }

        std::mutex fMutex;
        std::condition_variable fCv;
        std::map<std::wstring, std::size_t> fStartedCounts;
        std::map<std::wstring, std::size_t> fCompletedCounts;
        std::set<std::wstring> fReleasedFiles;
        std::vector<std::wstring> fStartedFiles;
        bool fReleaseEverything = false;
    };

    std::unique_ptr<OIV::RequestProcessorType> MakeControlledProcessor(
        ControlledImageMagickResidencyProcessor*& outProcessor)
    {
        auto processor = std::make_unique<ControlledImageMagickResidencyProcessor>();
        outProcessor   = processor.get();
        return processor;
    }

    struct FileSessionControllerFixture
    {
        FileSessionControllerFixture(const OIV::Tests::GeneratedCorpus& corpus, const std::wstring& initialFile)
            : residency(MakeControlledProcessor(processor), 4),
              controller(
                  &activeProvider, &watcher, &sorter, corpus.extensions, corpus.extensionList, residency,
                  [this](const std::wstring& fileName, IMCodec::ImageSharedPtr image)
                  {
                      if (image != nullptr)
                          activeProvider.SetActiveFileName(fileName);
                      observer.MarkLoaded(fileName, image != nullptr);
                  },
                  [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&,
                     IMCodec::ImageSharedPtr) {})
        {
            activeProvider.SetActiveFileName(initialFile);
        }

        ~FileSessionControllerFixture() { processor->ReleaseAll(); }

        void LoadFolder() { controller.LoadFileInFolder(activeProvider.GetActiveFileName()); }

        ActiveFileProvider activeProvider;
        FakeFileWatcher watcher;
        OIV::FileSorter sorter;
        ControlledImageMagickResidencyProcessor* processor = nullptr;
        OIV::ImageResidency residency;
        BrowseObserver observer;
        OIV::FileSessionController controller;
    };
}  // namespace

TEST_CASE("FileSessionController sequentially browses ImageMagick generated files",
          "[ImageCompatibility][Integration][Browse]")
{
    const auto& corpus = OIV::Tests::EnsureImageMagickCorpus();
    const auto files   = OIV::Tests::FindConsecutiveValidFiles(corpus, 4);
    FileSessionControllerFixture fixture(corpus, files.front());

    fixture.LoadFolder();
    REQUIRE(fixture.controller.GetFileList().GetSize() == OIV::Tests::BuildBrowsingFileList(corpus).size());
    REQUIRE(fixture.controller.IsCurrentFile(files[0]));

    auto releaseAndWaitForCurrent = [&](const std::wstring& expectedFile)
    {
        INFO("Waiting for current file request: " << std::filesystem::path(expectedFile).string());
        REQUIRE(fixture.processor->WaitForStarted(expectedFile));
        fixture.processor->Release(expectedFile);
        REQUIRE(fixture.observer.WaitForLoaded(expectedFile));
        REQUIRE(fixture.controller.IsCurrentFile(expectedFile));
        REQUIRE(fixture.observer.CountLoaded(expectedFile) == 1);
        fixture.observer.Clear();
    };

    fixture.controller.RequestCurrentFileReload();
    releaseAndWaitForCurrent(files[0]);

    for (std::size_t i = 1; i < files.size(); ++i)
    {
        REQUIRE(fixture.controller.JumpFiles(1));
        REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == files[i]);
        releaseAndWaitForCurrent(files[i]);
    }

    for (std::size_t i = files.size() - 1; i > 0; --i)
    {
        REQUIRE(fixture.controller.JumpFiles(-1));
        REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == files[i - 1]);
        releaseAndWaitForCurrent(files[i - 1]);
    }
}

TEST_CASE("FileSessionController ignores stale pending loads during rapid ImageMagick browsing",
          "[ImageCompatibility][Integration][Browse]")
{
    const auto& corpus = OIV::Tests::EnsureImageMagickCorpus();
    const auto files   = OIV::Tests::FindConsecutiveValidFiles(corpus, 4);
    FileSessionControllerFixture fixture(corpus, files.front());

    fixture.LoadFolder();
    fixture.controller.RequestCurrentFileReload();

    REQUIRE(fixture.controller.JumpFiles(1));
    REQUIRE(fixture.controller.JumpFiles(1));
    REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == files[2]);

    REQUIRE(fixture.processor->WaitForStarted(files[0]));
    REQUIRE(fixture.processor->WaitForStarted(files[1]));
    REQUIRE(fixture.processor->WaitForStarted(files[2]));

    fixture.observer.Clear();
    fixture.processor->Release(files[0]);
    fixture.processor->Release(files[1]);

    REQUIRE(fixture.processor->WaitForCompleted(files[0]));
    REQUIRE(fixture.processor->WaitForCompleted(files[1]));
    REQUIRE(fixture.observer.WaitForNoLoaded(std::chrono::milliseconds(250)));

    fixture.processor->Release(files[2]);
    REQUIRE(fixture.observer.WaitForLoaded(files[2]));
    REQUIRE(fixture.observer.CountLoaded(files[0]) == 0);
    REQUIRE(fixture.observer.CountLoaded(files[1]) == 0);
    REQUIRE(fixture.observer.CountLoaded(files[2]) == 1);
    REQUIRE(fixture.controller.IsCurrentFile(files[2]));
}

TEST_CASE("FileSessionController ignores stale forward load after rapid backward ImageMagick browsing",
          "[ImageCompatibility][Integration][Browse]")
{
    const auto& corpus = OIV::Tests::EnsureImageMagickCorpus();
    const auto files   = OIV::Tests::FindConsecutiveValidFiles(corpus, 4);
    FileSessionControllerFixture fixture(corpus, files.front());

    fixture.LoadFolder();
    fixture.controller.RequestCurrentFileReload();

    REQUIRE(fixture.controller.JumpFiles(1));
    REQUIRE(fixture.controller.JumpFiles(1));
    REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == files[2]);
    REQUIRE(fixture.controller.JumpFiles(-1));
    REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == files[1]);

    REQUIRE(fixture.processor->WaitForStarted(files[1]));
    REQUIRE(fixture.processor->WaitForStarted(files[2]));

    fixture.observer.Clear();
    fixture.processor->Release(files[2]);
    REQUIRE(fixture.processor->WaitForCompleted(files[2]));
    REQUIRE(fixture.observer.WaitForNoLoaded(std::chrono::milliseconds(250)));

    fixture.processor->Release(files[1]);
    REQUIRE(fixture.observer.WaitForLoaded(files[1]));
    REQUIRE(fixture.observer.CountLoaded(files[2]) == 0);
    REQUIRE(fixture.observer.CountLoaded(files[1]) == 1);
    REQUIRE(fixture.controller.IsCurrentFile(files[1]));
}

TEST_CASE("FileSessionController recovers after browsing to a bad ImageMagick generated file",
          "[ImageCompatibility][Integration][Browse]")
{
    const auto& corpus              = OIV::Tests::EnsureImageMagickCorpus();
    const auto [validFile, badFile] = OIV::Tests::FindValidFileBeforeBadFile(corpus);
    FileSessionControllerFixture fixture(corpus, validFile);

    fixture.LoadFolder();
    REQUIRE(fixture.controller.IsCurrentFile(validFile));

    fixture.controller.RequestCurrentFileReload();
    REQUIRE(fixture.processor->WaitForStarted(validFile));
    fixture.processor->Release(validFile);
    REQUIRE(fixture.observer.WaitForLoaded(validFile));
    fixture.observer.Clear();

    REQUIRE(fixture.controller.JumpFiles(1));
    REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == badFile);
    REQUIRE(fixture.processor->WaitForStarted(badFile));

    fixture.processor->Release(badFile);
    REQUIRE(fixture.processor->WaitForCompleted(badFile));
    REQUIRE(fixture.observer.WaitForNoLoaded(std::chrono::milliseconds(250)));
    REQUIRE(fixture.controller.IsCurrentFile(badFile));

    REQUIRE(fixture.controller.JumpFiles(-1));
    REQUIRE(fixture.controller.GetFileList().GetCurrentItemName() == validFile);
    REQUIRE(fixture.observer.WaitForLoaded(validFile));
    REQUIRE(fixture.controller.IsCurrentFile(validFile));
}
