#include <LLUtils/StringDefs.h>
#include <catch2/catch_all.hpp>

#include <Image.h>
#include <ImageItem.h>
#include <OIVShared/BrowseResidencyController.h>
#include <OIVShared/ImageResidencyCache.h>
#include <OIVShared/Task.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    IMCodec::ImageSharedPtr CreateTestImage()
    {
        auto imageItem      = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType = IMCodec::ImageItemType::Image;
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    class FakeResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        explicit FakeResidencyProcessor(IMCodec::ImageSharedPtr image, bool shouldBlock = false)
            : fImage(std::move(image)), fShouldBlock(shouldBlock)
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey&,
                                     OIV::ImageResidencyCacheValue& outValue) override
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

        int GetCallCount() const { return fCallCount.load(); }

        void SetShouldSucceed(bool shouldSucceed) { fShouldSucceed = shouldSucceed; }

      private:

        IMCodec::ImageSharedPtr fImage;
        bool fShouldBlock                = false;
        std::atomic<int> fCallCount      = 0;
        std::atomic<bool> fShouldSucceed = true;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fStarted  = false;
        bool fReleased = false;
    };

    class ReloadResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        ReloadResidencyProcessor(IMCodec::ImageSharedPtr firstImage, IMCodec::ImageSharedPtr secondImage)
            : fFirstImage(std::move(firstImage)), fSecondImage(std::move(secondImage))
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey&,
                                     OIV::ImageResidencyCacheValue& outValue) override
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

        int GetCallCount() const { return fCallCount.load(); }

      private:

        IMCodec::ImageSharedPtr fFirstImage;
        IMCodec::ImageSharedPtr fSecondImage;
        std::atomic<int> fCallCount = 0;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fFirstStarted  = false;
        bool fFirstReleased = false;
    };

    class SelectiveResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        explicit SelectiveResidencyProcessor(std::vector<LLUtils::native_string_type> successfulFiles)
            : fSuccessfulFiles(std::move(successfulFiles))
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey& key,
                                     OIV::ImageResidencyCacheValue& outValue) override
        {
            ++fCallCount;

            const auto& fileName = std::get<0>(key);
            if (std::find(fSuccessfulFiles.begin(), fSuccessfulFiles.end(), fileName) == fSuccessfulFiles.end())
                return false;

            outValue = CreateTestImage();
            return true;
        }

        int GetCallCount() const { return fCallCount.load(); }

      private:

        std::vector<LLUtils::native_string_type> fSuccessfulFiles;
        std::atomic<int> fCallCount = 0;
    };

    class FolderSwitchResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        explicit FolderSwitchResidencyProcessor(IMCodec::ImageSharedPtr image) : fImage(std::move(image)) {}

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey& key,
                                     OIV::ImageResidencyCacheValue& outValue) override
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

        int GetCallCount() const { return fCallCount.load(); }

      private:

        IMCodec::ImageSharedPtr fImage;
        std::atomic<int> fCallCount = 0;

        std::mutex fMutex;
        std::condition_variable fStartedCv;
        std::condition_variable fReleaseCv;
        bool fFolderAStarted  = false;
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

    std::jthread worker(
        [task]() mutable
        {
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

    PriorityTaskExecutor<int, int> executor(1,
                                            [&](const int request)
                                            {
                                                std::lock_guard lock(processedMutex);
                                                processedOrder.push_back(request);
                                                return request;
                                            });

    auto firstTask  = executor.Submit(1, 0);
    auto secondTask = executor.Submit(2, 0);

    REQUIRE(firstTask.get() == 1);
    REQUIRE(secondTask.get() == 2);
    REQUIRE(processedOrder == std::vector<int>{1, 2});
}

TEST_CASE("ImageResidencyCache coalesces in-flight requests and caches successful results", "[Residency]")
{
    auto expectedImage                   = CreateTestImage();
    auto processor                       = std::make_unique<FakeResidencyProcessor>(expectedImage, true);
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    OIV::TicketID firstTask;
    REQUIRE_NOTHROW(firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"),
                                                                OIV::ImageResidencyCacheItemType::FullSize));
    REQUIRE_NOTHROW(processorPtr->WaitUntilStarted());

    OIV::TicketID secondTask;
    REQUIRE_NOTHROW(secondTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"),
                                                                 OIV::ImageResidencyCacheItemType::FullSize));
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
    REQUIRE_NOTHROW(cachedTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-a"),
                                                                 OIV::ImageResidencyCacheItemType::FullSize));
    REQUIRE(cachedTask.await_ready() == true);
    IMCodec::ImageSharedPtr cachedImage;
    REQUIRE_NOTHROW(cachedImage = cachedTask.get());
    REQUIRE(cachedImage == expectedImage);
    REQUIRE(processorPtr->GetCallCount() == 1);
}

TEST_CASE("ImageResidencyCache retries after a failed request", "[Residency]")
{
    auto expectedImage                   = CreateTestImage();
    auto processor                       = std::make_unique<FakeResidencyProcessor>(expectedImage);
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    processorPtr->SetShouldSucceed(false);
    OIV::TicketID failedTask;
    REQUIRE_NOTHROW(failedTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-b"),
                                                                 OIV::ImageResidencyCacheItemType::Thumbnail));
    IMCodec::ImageSharedPtr failedImage;
    REQUIRE_NOTHROW(failedImage = failedTask.get());
    REQUIRE(failedImage == nullptr);
    REQUIRE(processorPtr->GetCallCount() == 1);

    processorPtr->SetShouldSucceed(true);
    OIV::TicketID retryTask;
    REQUIRE_NOTHROW(retryTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-b"),
                                                                OIV::ImageResidencyCacheItemType::Thumbnail));
    IMCodec::ImageSharedPtr retryImage;
    REQUIRE_NOTHROW(retryImage = retryTask.get());
    REQUIRE(retryImage == expectedImage);
    REQUIRE(processorPtr->GetCallCount() == 2);
}

TEST_CASE("ImageResidencyCache removes resident items from the cache", "[Residency]")
{
    auto expectedImage = CreateTestImage();
    auto processor     = std::make_unique<FakeResidencyProcessor>(expectedImage);
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    auto firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-c"),
                                                     OIV::ImageResidencyCacheItemType::FullSize);
    REQUIRE(firstTask.get() == expectedImage);
    REQUIRE(residency.getResidencyState(LLUTILS_TEXT("image-c"), OIV::ImageResidencyCacheItemType::FullSize) ==
            OIV::ResidencyState::Resident);

    residency.removeResidency(LLUTILS_TEXT("image-c"), OIV::ImageResidencyCacheItemType::FullSize);
    REQUIRE(residency.getResidencyState(LLUTILS_TEXT("image-c"), OIV::ImageResidencyCacheItemType::FullSize) ==
            OIV::ResidencyState::NotResident);
}

TEST_CASE("ImageResidencyCache reload bypasses stale in-flight requests", "[Residency]")
{
    auto firstImage                        = CreateTestImage();
    auto secondImage                       = CreateTestImage();
    auto processor                         = std::make_unique<ReloadResidencyProcessor>(firstImage, secondImage);
    ReloadResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 2);

    auto firstTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"),
                                                     OIV::ImageResidencyCacheItemType::FullSize);
    processorPtr->WaitUntilFirstStarted();

    residency.removeResidency(LLUTILS_TEXT("image-reload"), OIV::ImageResidencyCacheItemType::FullSize);
    auto secondTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"),
                                                      OIV::ImageResidencyCacheItemType::FullSize);

    REQUIRE(WaitUntil([&] { return processorPtr->GetCallCount() >= 2; }));
    REQUIRE(secondTask.get() == secondImage);

    processorPtr->ReleaseFirst();
    REQUIRE(firstTask.get() == firstImage);

    auto cachedTask = residency.requestResidencyAsync(LLUTILS_TEXT("image-reload"),
                                                      OIV::ImageResidencyCacheItemType::FullSize);
    REQUIRE(cachedTask.await_ready() == true);
    REQUIRE(cachedTask.get() == secondImage);
    REQUIRE(processorPtr->GetCallCount() == 2);
}

TEST_CASE("BrowseResidencyController predicts forward and bounds retained residency", "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<LLUtils::native_string_type> currentReadyFiles;

    OIV::BrowseResidencyController manager(
        residency,
        [&](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr)
        {
            std::lock_guard lock(callbackMutex);
            currentReadyFiles.push_back(fileName);
            callbackCv.notify_all();
        },
        [](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type&,
           IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyController::FolderFileListSnapshot fileList{
        LLUTILS_TEXT("folder-a"),
        {LLUTILS_TEXT("a"), LLUTILS_TEXT("b"), LLUTILS_TEXT("c"), LLUTILS_TEXT("d"), LLUTILS_TEXT("e"),
         LLUTILS_TEXT("f"), LLUTILS_TEXT("g")},
        0};

    manager.RefreshCommittedCurrent(fileList);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("b"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident &&
                   residency.getResidencyState(LLUTILS_TEXT("c"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident;
        }));

    auto secondSnapshot         = fileList;
    secondSnapshot.currentIndex = 1;
    manager.CommitNavigation(secondSnapshot, 0);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("d"), OIV::ImageResidencyCacheItemType::FullSize) ==
                   OIV::ResidencyState::Resident;
        }));

    auto thirdSnapshot         = fileList;
    thirdSnapshot.currentIndex = 2;
    manager.CommitNavigation(thirdSnapshot, 1);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("e"), OIV::ImageResidencyCacheItemType::FullSize) ==
                   OIV::ResidencyState::Resident;
        }));

    auto fourthSnapshot         = fileList;
    fourthSnapshot.currentIndex = 3;
    manager.CommitNavigation(fourthSnapshot, 2);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("f"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident &&
                   residency.getResidencyState(LLUTILS_TEXT("a"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::NotResident;
        }));

    REQUIRE_FALSE(WaitUntil(
        [&]
        {
            std::lock_guard lock(callbackMutex);
            return currentReadyFiles.empty() == false;
        },
        std::chrono::milliseconds(100)));
}

TEST_CASE("BrowseResidencyController reverses prediction direction and reloads through residency",
          "[Residency][BrowsePolicy]")
{
    auto processor                       = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    std::atomic<int> currentReadyCount = 0;
    OIV::BrowseResidencyController manager(
        residency, [&](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) { ++currentReadyCount; },
        [](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type&,
           IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyController::FolderFileListSnapshot fileList{LLUTILS_TEXT("folder-a"),
                                                                          {LLUTILS_TEXT("a"), LLUTILS_TEXT("b"),
                                                                           LLUTILS_TEXT("c"), LLUTILS_TEXT("d"),
                                                                           LLUTILS_TEXT("e"), LLUTILS_TEXT("f")},
                                                                          4};

    manager.CommitNavigation(fileList, 3);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("f"), OIV::ImageResidencyCacheItemType::FullSize) ==
                   OIV::ResidencyState::Resident;
        }));

    auto reversedSnapshot         = fileList;
    reversedSnapshot.currentIndex = 3;
    manager.CommitNavigation(reversedSnapshot, 4);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("c"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident &&
                   residency.getResidencyState(LLUTILS_TEXT("b"), OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident;
        }));

    const int callsBeforeReload = processorPtr->GetCallCount();
    manager.ReloadCurrent(reversedSnapshot);
    REQUIRE(WaitUntil([&] { return processorPtr->GetCallCount() > callsBeforeReload; }));
    REQUIRE(WaitUntil([&] { return currentReadyCount.load() > 0; }));
}

TEST_CASE("BrowseResidencyController reports current image load failures", "[Residency][BrowsePolicy]")
{
    auto processor                       = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    FakeResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    processorPtr->SetShouldSucceed(false);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<std::pair<LLUtils::native_string_type, IMCodec::ImageSharedPtr>> currentReadyResults;

    OIV::BrowseResidencyController manager(
        residency,
        [&](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
        {
            std::lock_guard lock(callbackMutex);
            currentReadyResults.emplace_back(fileName, image);
            callbackCv.notify_all();
        },
        [](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type&,
           IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyController::FolderFileListSnapshot fileList{LLUTILS_TEXT("folder-a"),
                                                                          {LLUTILS_TEXT("failed-image")},
                                                                          0};

    manager.ReloadCurrent(fileList);

    std::unique_lock callbackLock(callbackMutex);
    REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500),
                                [&] { return currentReadyResults.empty() == false; }));
    REQUIRE(currentReadyResults.front().first == LLUTILS_TEXT("failed-image"));
    REQUIRE(currentReadyResults.front().second == nullptr);
}

TEST_CASE("BrowseResidencyController ties residency eviction to working folder", "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<FakeResidencyProcessor>(CreateTestImage());
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    OIV::BrowseResidencyController manager(
        residency, [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type&,
           IMCodec::ImageSharedPtr) {});

    const OIV::BrowseResidencyController::FolderFileListSnapshot fileListA{
        LLUTILS_TEXT("folder-a"),
        {LLUTILS_TEXT("folder-a\\a"), LLUTILS_TEXT("folder-a\\b"), LLUTILS_TEXT("folder-a\\c"),
         LLUTILS_TEXT("folder-a\\d")},
        1};

    manager.RefreshCommittedCurrent(fileListA);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("folder-a\\c"),
                                               OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident &&
                   residency.getResidencyState(LLUTILS_TEXT("folder-a\\d"),
                                               OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::Resident;
        }));

    manager.InvalidateCurrent();
    REQUIRE(residency.getResidencyState(LLUTILS_TEXT("folder-a\\c"), OIV::ImageResidencyCacheItemType::FullSize) ==
            OIV::ResidencyState::Resident);

    const OIV::BrowseResidencyController::FolderFileListSnapshot fileListB{LLUTILS_TEXT("folder-b"),
                                                                           {LLUTILS_TEXT("folder-b\\a"),
                                                                            LLUTILS_TEXT("folder-b\\b")},
                                                                           0};
    manager.RefreshCommittedCurrent(fileListB);
    REQUIRE(WaitUntil(
        [&]
        {
            return residency.getResidencyState(LLUTILS_TEXT("folder-a\\c"),
                                               OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::NotResident &&
                   residency.getResidencyState(LLUTILS_TEXT("folder-a\\d"),
                                               OIV::ImageResidencyCacheItemType::FullSize) ==
                       OIV::ResidencyState::NotResident;
        }));
}

TEST_CASE("BrowseResidencyController ignores stale folder-load completions", "[Residency][BrowsePolicy]")
{
    auto processor                               = std::make_unique<FolderSwitchResidencyProcessor>(CreateTestImage());
    FolderSwitchResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 2);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<LLUtils::native_string_type> callbackFolders;

    OIV::BrowseResidencyController manager(
        residency, [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseResidencyController::FolderFileListSnapshot& snapshot, const LLUtils::native_string_type&,
            IMCodec::ImageSharedPtr)
        {
            std::lock_guard lock(callbackMutex);
            callbackFolders.push_back(snapshot.folderPath);
            callbackCv.notify_all();
        });

    const OIV::BrowseResidencyController::FolderFileListSnapshot folderA{
        LLUTILS_TEXT("folder-a"),
        {LLUTILS_TEXT("folder-a\\a")},
        OIV::BrowseResidencyController::FolderFileListSnapshot::IndexStart};
    const OIV::BrowseResidencyController::FolderFileListSnapshot folderB{
        LLUTILS_TEXT("folder-b"),
        {LLUTILS_TEXT("folder-b\\a")},
        OIV::BrowseResidencyController::FolderFileListSnapshot::IndexStart};

    manager.RequestFolderLoadResidency(folderA);
    processorPtr->WaitUntilFolderAStarted();

    manager.RequestFolderLoadResidency(folderB);
    {
        std::unique_lock callbackLock(callbackMutex);
        REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500),
                                    [&]
                                    {
                                        return std::find(callbackFolders.begin(), callbackFolders.end(),
                                                         LLUTILS_TEXT("folder-b")) != callbackFolders.end();
                                    }));
    }

    processorPtr->ReleaseFolderA();
    REQUIRE_FALSE(WaitUntil(
        [&]
        {
            std::lock_guard lock(callbackMutex);
            return std::find(callbackFolders.begin(), callbackFolders.end(), LLUTILS_TEXT("folder-a")) !=
                   callbackFolders.end();
        },
        std::chrono::milliseconds(100)));
    REQUIRE(processorPtr->GetCallCount() >= 2);
}

TEST_CASE("BrowseResidencyController reports folder load failure after all candidates fail",
          "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<SelectiveResidencyProcessor>(std::vector<LLUtils::native_string_type>{});
    SelectiveResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<std::pair<LLUtils::native_string_type, IMCodec::ImageSharedPtr>> folderReadyResults;

    OIV::BrowseResidencyController manager(
        residency, [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type& fileName,
            IMCodec::ImageSharedPtr image)
        {
            std::lock_guard lock(callbackMutex);
            folderReadyResults.emplace_back(fileName, image);
            callbackCv.notify_all();
        });

    const OIV::BrowseResidencyController::FolderFileListSnapshot folder{
        LLUTILS_TEXT("folder-a"),
        {LLUTILS_TEXT("folder-a\\a"), LLUTILS_TEXT("folder-a\\b")},
        OIV::BrowseResidencyController::FolderFileListSnapshot::IndexStart};

    manager.RequestFolderLoadResidency(folder);

    std::unique_lock callbackLock(callbackMutex);
    REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500),
                                [&] { return folderReadyResults.empty() == false; }));
    REQUIRE(folderReadyResults.size() == 1);
    REQUIRE(folderReadyResults.front().first == LLUTILS_TEXT("folder-a\\a"));
    REQUIRE(folderReadyResults.front().second == nullptr);
    REQUIRE(processorPtr->GetCallCount() == 2);
}

TEST_CASE("BrowseResidencyController keeps scanning folder failures until a candidate loads",
          "[Residency][BrowsePolicy]")
{
    auto processor = std::make_unique<SelectiveResidencyProcessor>(
        std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("folder-a\\b")});
    SelectiveResidencyProcessor* processorPtr = processor.get();
    OIV::ImageResidencyCache residency(std::move(processor), 1);

    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::vector<std::pair<LLUtils::native_string_type, IMCodec::ImageSharedPtr>> folderReadyResults;

    OIV::BrowseResidencyController manager(
        residency, [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseResidencyController::FolderFileListSnapshot&, const LLUtils::native_string_type& fileName,
            IMCodec::ImageSharedPtr image)
        {
            std::lock_guard lock(callbackMutex);
            folderReadyResults.emplace_back(fileName, image);
            callbackCv.notify_all();
        });

    const OIV::BrowseResidencyController::FolderFileListSnapshot folder{
        LLUTILS_TEXT("folder-a"),
        {LLUTILS_TEXT("folder-a\\a"), LLUTILS_TEXT("folder-a\\b")},
        OIV::BrowseResidencyController::FolderFileListSnapshot::IndexStart};

    manager.RequestFolderLoadResidency(folder);

    std::unique_lock callbackLock(callbackMutex);
    REQUIRE(callbackCv.wait_for(callbackLock, std::chrono::milliseconds(500),
                                [&] { return folderReadyResults.empty() == false; }));
    REQUIRE(folderReadyResults.size() == 1);
    REQUIRE(folderReadyResults.front().first == LLUTILS_TEXT("folder-a\\b"));
    REQUIRE(folderReadyResults.front().second != nullptr);
    REQUIRE(processorPtr->GetCallCount() == 2);
}
