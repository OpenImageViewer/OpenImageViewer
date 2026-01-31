#include <catch2/catch_all.hpp>

#include <Image.h>
#include <ImageItem.h>
#include <oivshared/BrowseResidencyManager.h>
#include <oivshared/ImageResidency.h>
#include <oivshared/Task.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    IMCodec::ImageSharedPtr CreateTestImage()
    {
        auto imageItem = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType = IMCodec::ImageItemType::Image;
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    class FakeResidencyProcessor : public OIV::RequestProcessorType
    {
      public:
        explicit FakeResidencyProcessor(IMCodec::ImageSharedPtr image, bool shouldBlock = false)
            : fImage(std::move(image))
            , fShouldBlock(shouldBlock)
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyKey&, OIV::ImageResidencyValue& outValue) override
        {
            ++fCallCount;

            std::unique_lock lock(fMutex);
            fStarted = true;
            fStartedCv.notify_all();

            if (fShouldBlock)
                fReleaseCv.wait(lock, [&] { return fReleased; });

            lock.unlock();

            if (fShouldSucceed == false)
                return false;

            outValue = fImage;
            return true;
        }

        void WaitUntilStarted()
        {
            std::unique_lock lock(fMutex);
            fStartedCv.wait(lock, [&] { return fStarted; });
        }

        void Release()
        {
            if (fShouldBlock)
            {
                std::lock_guard lock(fMutex);
                fReleased = true;
            }

            fReleaseCv.notify_all();
        }

        int GetCallCount() const
        {
            return fCallCount.load();
        }

        void SetShouldSucceed(bool shouldSucceed)
        {
            fShouldSucceed = shouldSucceed;
        }

      private:
        IMCodec::ImageSharedPtr fImage;
        bool fShouldBlock = false;
        std::atomic<int> fCallCount = 0;
        std::atomic<bool> fShouldSucceed = true;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fStarted = false;
        bool fReleased = false;
    };

    class ReloadResidencyProcessor : public OIV::RequestProcessorType
    {
      public:
        ReloadResidencyProcessor(IMCodec::ImageSharedPtr firstImage, IMCodec::ImageSharedPtr secondImage)
            : fFirstImage(std::move(firstImage))
            , fSecondImage(std::move(secondImage))
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyKey&, OIV::ImageResidencyValue& outValue) override
        {
            const int callIndex = ++fCallCount;

            if (callIndex == 1)
            {
                {
                    std::lock_guard lock(fMutex);
                    fFirstStarted = true;
                }

                fStartedCv.notify_all();

                std::unique_lock lock(fMutex);
                fReleaseCv.wait(lock, [&] { return fFirstReleased; });
                outValue = fFirstImage;
                return true;
            }

            outValue = fSecondImage;
            return true;
        }

        void WaitUntilFirstStarted()
        {
            std::unique_lock lock(fMutex);
            fStartedCv.wait(lock, [&] { return fFirstStarted; });
        }

        void ReleaseFirst()
        {
            {
                std::lock_guard lock(fMutex);
                fFirstReleased = true;
            }

            fReleaseCv.notify_all();
        }

        int GetCallCount() const
        {
            return fCallCount.load();
        }

      private:
        IMCodec::ImageSharedPtr fFirstImage;
        IMCodec::ImageSharedPtr fSecondImage;
        std::atomic<int> fCallCount = 0;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fFirstStarted = false;
        bool fFirstReleased = false;
    };

    class FolderSwitchResidencyProcessor : public OIV::RequestProcessorType
    {
      public:
        explicit FolderSwitchResidencyProcessor(IMCodec::ImageSharedPtr image)
            : fImage(std::move(image))
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyKey& key, OIV::ImageResidencyValue& outValue) override
        {
            ++fCallCount;

            if (std::get<0>(key) == LLUTILS_TEXT("folder-a\\a"))
            {
                {
                    std::lock_guard lock(fMutex);
                    fFolderAStarted = true;
                }

                fStartedCv.notify_all();

                std::unique_lock lock(fMutex);
                fReleaseCv.wait(lock, [&] { return fFolderAReleased; });
            }

            outValue = fImage;
            return true;
        }

        void WaitUntilFolderAStarted()
        {
            std::unique_lock lock(fMutex);
            fStartedCv.wait(lock, [&] { return fFolderAStarted; });
        }

        void ReleaseFolderA()
        {
            {
                std::lock_guard lock(fMutex);
                fFolderAReleased = true;
            }

            fReleaseCv.notify_all();
        }

        int GetCallCount() const
        {
            return fCallCount.load();
        }

      private:
        IMCodec::ImageSharedPtr fImage;
        std::atomic<int> fCallCount = 0;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fFolderAStarted = false;
        bool fFolderAReleased = false;
    };

    bool WaitUntil(const std::function<bool()>& predicate,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return predicate();
    }
}  // namespace

TEST_CASE("Task copies share completion state", "[Task]")
{
    Task<int> task = Task<int>::CreatePending();
    Task<int> copy = task;

    std::jthread worker([task]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        task.SetValue(42);
    });

    REQUIRE(task.await_ready() == false);
    REQUIRE(copy.get() == 42);
    REQUIRE(task.get() == 42);
    REQUIRE(task.await_ready() == true);
}

TEST_CASE("PriorityTaskExecutor preserves submission order for equal priorities", "[TaskExecutor]")
{
    std::mutex processedMutex;
    std::vector<int> processedOrder;

    PriorityTaskExecutor<int, int> executor(1, [&](const int request) {
        std::lock_guard lock(processedMutex);
        processedOrder.push_back(request);
        return request;
    });

    auto firstTask = executor.Submit(1, 0);
    auto secondTask = executor.Submit(2, 0);

    REQUIRE(firstTask.get() == 1);
    REQUIRE(secondTask.get() == 2);
    REQUIRE(processedOrder == std::vector<int>{1, 2});
}

TEST_CASE("ImageResidency coalesces in-flight requests and caches successful results", "[Residency]")
{
    auto expectedImage = CreateTestImage();
    auto processor = std::make_unique<FakeResidencyProcessor>(expectedImage, true);
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidency residency(std::move(processor), 1);

    OIV::TicketID firstTask;
    REQUIRE_NOTHROW(firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"), OIV::ImageResidencyItemType::FullSize));
    REQUIRE_NOTHROW(processorPtr->WaitUntilStarted());

    OIV::TicketID secondTask;
    REQUIRE_NOTHROW(secondTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"), OIV::ImageResidencyItemType::FullSize));
    REQUIRE(processorPtr->GetCallCount() == 1);

    REQUIRE_NOTHROW(processorPtr->Release());

    IMCodec::ImageSharedPtr firstImage;
    IMCodec::ImageSharedPtr secondImage;
    REQUIRE_NOTHROW(firstImage = firstTask.get());
    REQUIRE_NOTHROW(secondImage = secondTask.get());
    REQUIRE(firstImage == expectedImage);
    REQUIRE(secondImage == expectedImage);
    REQUIRE(processorPtr->GetCallCount() == 1);

    OIV::TicketID cachedTask;
    REQUIRE_NOTHROW(cachedTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"), OIV::ImageResidencyItemType::FullSize));
    REQUIRE(cachedTask.await_ready() == true);
    IMCodec::ImageSharedPtr cachedImage;
    REQUIRE_NOTHROW(cachedImage = cachedTask.get());
    REQUIRE(cachedImage == expectedImage);
    REQUIRE(processorPtr->GetCallCount() == 1);
}

TEST_CASE("ImageResidency retries after a failed request", "[Residency]")
{
    auto expectedImage = CreateTestImage();
    auto processor = std::make_unique<FakeResidencyProcessor>(expectedImage);
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidency residency(std::move(processor), 1);

    processorPtr->SetShouldSucceed(false);
    OIV::TicketID failedTask;
    REQUIRE_NOTHROW(failedTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-b"), OIV::ImageResidencyItemType::Thumbnail));
    IMCodec::ImageSharedPtr failedImage;
    REQUIRE_NOTHROW(failedImage = failedTask.get());
    REQUIRE(failedImage == nullptr);
    REQUIRE(processorPtr->GetCallCount() == 1);

    processorPtr->SetShouldSucceed(true);
    OIV::TicketID retryTask;
    REQUIRE_NOTHROW(retryTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-b"), OIV::ImageResidencyItemType::Thumbnail));
    IMCodec::ImageSharedPtr retryImage;
    REQUIRE_NOTHROW(retryImage = retryTask.get());
    REQUIRE(retryImage == expectedImage);
    REQUIRE(processorPtr->GetCallCount() == 2);
}

TEST_CASE("ImageResidency removes resident items from the cache", "[Residency]")
{
    auto expectedImage = CreateTestImage();
    auto processor = std::make_unique<FakeResidencyProcessor>(expectedImage);
    OIV::ImageResidency residency(std::move(processor), 1);

    auto firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-c"), OIV::ImageResidencyItemType::FullSize);
    REQUIRE(firstTask.get() == expectedImage);
    REQUIRE(residency.getResidencyState(LLUTILS_TEXT("image-c"), OIV::ImageResidencyItemType::FullSize) ==
            OIV::ResidencyState::Resident);

    residency.removeResidency(LLUTILS_TEXT("image-c"), OIV::ImageResidencyItemType::FullSize);
    REQUIRE(residency.getResidencyState(LLUTILS_TEXT("image-c"), OIV::ImageResidencyItemType::FullSize) ==
            OIV::ResidencyState::NotResident);
}

TEST_CASE("ImageResidency reload bypasses stale in-flight requests", "[Residency]")
{
    auto firstImage = CreateTestImage();
    auto secondImage = CreateTestImage();
    auto processor = std::make_unique<ReloadResidencyProcessor>(firstImage, secondImage);
    ReloadResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidency residency(std::move(processor), 2);

    auto firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"), OIV::ImageResidencyItemType::FullSize);
    processorPtr->WaitUntilFirstStarted();

    residency.removeResidency(LLUTILS_TEXT("image-reload"), OIV::ImageResidencyItemType::FullSize);
    auto secondTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"), OIV::ImageResidencyItemType::FullSize);

    REQUIRE(WaitUntil([&] { return processorPtr->GetCallCount() >= 2; }));
    REQUIRE(secondTask.get() == secondImage);

    processorPtr->ReleaseFirst();
    REQUIRE(firstTask.get() == firstImage);

    auto cachedTask =
        residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"), OIV::ImageResidencyItemType::FullSize);
    REQUIRE(cachedTask.await_ready() == true);
    REQUIRE(cachedTask.get() == secondImage);
    REQUIRE(processorPtr->GetCallCount() == 2);
}

TEST_CASE("BrowseResidencyManager predicts forward and bounds retained residency", "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    OIV::ImageResidency residency(std::move(processor), 1);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<std::wstring> currentReadyFiles;

    OIV::BrowseResidencyManager manager(
        residency,
        [&](const std::wstring& fileName, IMCodec::ImageSharedPtr)
        {
            std::lock_guard lock(callbackMutex);
            currentReadyFiles.push_back(fileName);
            callbackCv.notify_all();
        },
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyManager::FileListSnapshot fileList{
        L"folder-a", {L"a", L"b", L"c", L"d", L"e", L"f", L"g"}, 0
    };

    manager.OnCurrentIndexChanged(fileList, 0);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"a", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident &&
               residency.getResidencyState(L"b", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident &&
               residency.getResidencyState(L"c", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident;
    }));

    auto secondSnapshot = fileList;
    secondSnapshot.currentIndex = 1;
    manager.OnCurrentIndexChanged(secondSnapshot, 0);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"d", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident;
    }));

    auto thirdSnapshot = fileList;
    thirdSnapshot.currentIndex = 2;
    manager.OnCurrentIndexChanged(thirdSnapshot, 1);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"e", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident;
    }));

    auto fourthSnapshot = fileList;
    fourthSnapshot.currentIndex = 3;
    manager.OnCurrentIndexChanged(fourthSnapshot, 2);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"f", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident &&
               residency.getResidencyState(L"a", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::NotResident;
    }));

    std::unique_lock callbackLock(callbackMutex);
    REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500),
                                [&] { return std::find(currentReadyFiles.begin(), currentReadyFiles.end(), L"d") !=
                                                 currentReadyFiles.end(); }));
}

TEST_CASE("BrowseResidencyManager reverses prediction direction and reloads through residency",
          "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidency residency(std::move(processor), 1);

    std::atomic<int> currentReadyCount = 0;
    OIV::BrowseResidencyManager manager(
        residency,
        [&](const std::wstring&, IMCodec::ImageSharedPtr) { ++currentReadyCount; },
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyManager::FileListSnapshot fileList{
        L"folder-a", {L"a", L"b", L"c", L"d", L"e", L"f"}, 4
    };

    manager.OnCurrentIndexChanged(fileList, 3);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"e", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident;
    }));

    auto reversedSnapshot = fileList;
    reversedSnapshot.currentIndex = 3;
    manager.OnCurrentIndexChanged(reversedSnapshot, 4);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"c", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident &&
               residency.getResidencyState(L"b", OIV::ImageResidencyItemType::FullSize) == OIV::ResidencyState::Resident;
    }));

    const int callsBeforeReload = processorPtr->GetCallCount();
    manager.OnCurrentFileReloadRequested(reversedSnapshot);
    REQUIRE(WaitUntil([&] { return processorPtr->GetCallCount() > callsBeforeReload; }));
    REQUIRE(currentReadyCount.load() > 0);
}

TEST_CASE("BrowseResidencyManager ties residency eviction to working folder", "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    OIV::ImageResidency residency(std::move(processor), 1);

    OIV::BrowseResidencyManager manager(
        residency,
        [](const std::wstring&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyManager::FileListSnapshot fileListA{
        L"folder-a", {L"folder-a\\a", L"folder-a\\b", L"folder-a\\c", L"folder-a\\d"}, 1
    };

    manager.OnCurrentIndexChanged(fileListA, 0);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"folder-a\\b", OIV::ImageResidencyItemType::FullSize) ==
                   OIV::ResidencyState::Resident &&
               residency.getResidencyState(L"folder-a\\c", OIV::ImageResidencyItemType::FullSize) ==
                   OIV::ResidencyState::Resident;
    }));

    manager.InvalidateCurrent();
    REQUIRE(residency.getResidencyState(L"folder-a\\b", OIV::ImageResidencyItemType::FullSize) ==
            OIV::ResidencyState::Resident);

    const OIV::BrowseResidencyManager::FileListSnapshot fileListB{
        L"folder-b", {L"folder-b\\a", L"folder-b\\b"}, 0
    };
    manager.OnCurrentIndexChanged(fileListB, OIV::BrowseResidencyManager::FileListSnapshot::IndexStart);
    REQUIRE(WaitUntil([&]
    {
        return residency.getResidencyState(L"folder-a\\b", OIV::ImageResidencyItemType::FullSize) ==
                   OIV::ResidencyState::NotResident &&
               residency.getResidencyState(L"folder-a\\c", OIV::ImageResidencyItemType::FullSize) ==
                   OIV::ResidencyState::NotResident;
    }));
}

TEST_CASE("BrowseResidencyManager ignores stale folder-load completions", "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FolderSwitchResidencyProcessor>(CreateTestImage());
    FolderSwitchResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidency residency(std::move(processor), 2);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<std::wstring> callbackFolders;

    OIV::BrowseResidencyManager manager(
        residency,
        [](const std::wstring&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseResidencyManager::FileListSnapshot& snapshot,
            const std::wstring&,
            IMCodec::ImageSharedPtr)
        {
            std::lock_guard lock(callbackMutex);
            callbackFolders.push_back(snapshot.folderPath);
            callbackCv.notify_all();
        });

    const OIV::BrowseResidencyManager::FileListSnapshot folderA{
        L"folder-a", {L"folder-a\\a"}, OIV::BrowseResidencyManager::FileListSnapshot::IndexStart
    };
    const OIV::BrowseResidencyManager::FileListSnapshot folderB{
        L"folder-b", {L"folder-b\\a"}, OIV::BrowseResidencyManager::FileListSnapshot::IndexStart
    };

    manager.RequestFolderLoadResidency(folderA);
    processorPtr->WaitUntilFolderAStarted();

    manager.RequestFolderLoadResidency(folderB);
    {
        std::unique_lock callbackLock(callbackMutex);
        REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500), [&] {
            return std::find(callbackFolders.begin(), callbackFolders.end(), L"folder-b") != callbackFolders.end();
        }));
    }

    processorPtr->ReleaseFolderA();
    REQUIRE_FALSE(WaitUntil([&]
    {
        std::lock_guard lock(callbackMutex);
        return std::find(callbackFolders.begin(), callbackFolders.end(), L"folder-a") != callbackFolders.end();
    }, std::chrono::milliseconds(100)));
    REQUIRE(processorPtr->GetCallCount() >= 2);
}
