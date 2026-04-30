#include <catch2/catch_all.hpp>

#include <oivappcore/CommandManager.h>
#include <oivappcore/FileList.h>
#include <oivappcore/FileSessionController.h>
#include <oivappcore/ImageLoadController.h>
#include <oivappcore/IFileWatcher.h>
#include <oivappcore/SelectionRect.h>
#include <oivshared/ViewTransformController.h>
#include <oivshared/AdaptiveMotion.h>
#include <oivshared/FileSorter.h>
#include <oivshared/ImageResidency.h>
#include <oivshared/RecursiveDelayOp.h>

#include <Image.h>
#include <ImageItem.h>
#include <LLUtils/StringUtility.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
    class FakeFileWatcher : public OIV::IFileWatcher
    {
      public:
        bool IsFolderRegistered(const std::wstring& folder) const override
        {
            return fFolder == folder;
        }

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

        OnFileChangedEventArgsEvent& GetFileChangedEvent() override
        {
            return fEvent;
        }

        void Raise(FileChangedOp op, const std::wstring& folder, const std::wstring& fileName, const std::wstring& fileName2 = {})
        {
            fEvent.Raise(FileChangedEventArgs{fFolderID, op, folder, fileName, fileName2});
        }

      private:
        FolderID fFolderID = 7;
        std::wstring fFolder;
        OnFileChangedEventArgsEvent fEvent;
    };

    class ActiveFileProvider : public OIV::IFileListProvider
    {
      public:
        OIV::FileListStringType GetActiveFileName() override
        {
            return activeFile;
        }

        OIV::FileListStringType activeFile;
    };

    IMCodec::ImageSharedPtr CreateTestImage(uint32_t width = 1, uint32_t height = 1)
    {
        auto imageItem = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType = IMCodec::ImageItemType::Image;
        imageItem->descriptor.width = width;
        imageItem->descriptor.height = height;
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    class CountingResidencyProcessor : public OIV::RequestProcessorType
    {
      public:
        bool ProcessResidencyRequest(const OIV::ImageResidencyKey&, OIV::ImageResidencyValue& outValue) override
        {
            ++fCallCount;
            outValue = CreateTestImage();
            return true;
        }

        int GetCallCount() const
        {
            return fCallCount;
        }

      private:
        std::atomic<int> fCallCount = 0;
    };

    std::filesystem::path MakeTempFolder(const char* name)
    {
        auto folder = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(folder);
        std::filesystem::create_directories(folder);
        return folder;
    }

    void TouchFile(const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "test";
    }

    class FakeImageFileLoader : public OIV::IImageFileLoader
    {
      public:
        OIV::ImageFileLoadResult LoadFile(const std::wstring& normalizedFilePath,
                                          IMCodec::PluginTraverseMode traverseMode,
                                          const OIV::ImageLoadContext& context) override
        {
            lastPath = normalizedFilePath;
            lastTraverseMode = traverseMode;
            lastContext = context;

            return OIV::ImageFileLoadResult{
                resultCode,
                image != nullptr ? std::make_shared<OIV::OIVFileImage>(normalizedFilePath, image) : nullptr};
        }

        ResultCode resultCode = ResultCode::RC_Success;
        IMCodec::ImageSharedPtr image = CreateTestImage();
        std::wstring lastPath;
        IMCodec::PluginTraverseMode lastTraverseMode = IMCodec::PluginTraverseMode::NoTraverse;
        OIV::ImageLoadContext lastContext;
    };
}

TEST_CASE("RecursiveDelayedOp delays callbacks until the outer operation ends", "[AppCore][Shared]")
{
    int callbackCount = 0;
    OIV::RecursiveDelayedOp operation([&]() { ++callbackCount; });

    operation.Begin();
    operation.Begin();
    operation.End();
    REQUIRE(callbackCount == 0);

    operation.End();
    REQUIRE(callbackCount == 1);

    operation.Begin();
    operation.End(false);
    REQUIRE(callbackCount == 1);
}

TEST_CASE("AdaptiveMotion can use deterministic elapsed time", "[AppCore][Shared]")
{
    double elapsedSeconds = 0.0;
    OIV::AdaptiveMotion motion(2.0, 1.0, 1.0, [&]() { return elapsedSeconds; });

    REQUIRE(motion.Add(1.0) == Catch::Approx(4.0));

    elapsedSeconds = 1.0;
    REQUIRE(motion.Add(1.0) == Catch::Approx(9.0));
}

TEST_CASE("CommandManager dispatches predefined command groups", "[AppCore]")
{
    OIV::CommandManager manager;
    bool executed = false;

    manager.AddCommand(OIV::CommandManager::Command(
        "open", [&](const OIV::CommandManager::CommandRequest& request, OIV::CommandManager::CommandResult& result) {
            executed = true;
            result.resValue = LLUtils::StringUtility::ToWString(request.args.GetArgValue("path"));
        }));

    manager.AddCommandGroup({"OpenImage", "Open image", "open", "path=image.png"});

    OIV::CommandManager::CommandResult result;
    REQUIRE(manager.ExecuteCommand(manager.GetCommandRequestGroup("OpenImage"), result));
    REQUIRE(executed);
    REQUIRE(result.resValue == L"image.png");
}

TEST_CASE("SelectionRect creates, moves, and cancels a selection", "[AppCore]")
{
    std::vector<std::pair<LLUtils::RectI32, bool>> changes;
    OIV::SelectionRect selection([&](const LLUtils::RectI32& rect, bool visible) {
        changes.push_back({rect, visible});
    });

    selection.SetSelection(OIV::SelectionRect::Operation::BeginDrag, {10, 10});
    selection.SetSelection(OIV::SelectionRect::Operation::Drag, {30, 40});
    selection.SetSelection(OIV::SelectionRect::Operation::EndDrag, {30, 40});

    REQUIRE(selection.GetSelectionRect().GetWidth() == 20);
    REQUIRE(selection.GetSelectionRect().GetHeight() == 30);
    REQUIRE(changes.back().second);

    selection.SetSelection(OIV::SelectionRect::Operation::BeginDrag, {20, 20});
    selection.SetSelection(OIV::SelectionRect::Operation::Drag, {25, 30});
    REQUIRE(selection.GetSelectionRect().GetCorner(LLUtils::Corner::TopLeft) == LLUtils::PointI32{15, 20});

    selection.SetSelection(OIV::SelectionRect::Operation::CancelSelection, {});
    REQUIRE(selection.GetSelectionRect().IsEmpty());
    REQUIRE_FALSE(changes.back().second);
}

TEST_CASE("ViewTransformController calculates fit, center, and coordinate transforms", "[AppCore]")
{
    REQUIRE(OIV::ViewTransformController::FitScale({800.0, 600.0}, {400.0, 400.0}) == Catch::Approx(1.5));

    const LLUtils::PointF64 offset = OIV::ViewTransformController::CenterOffset({400.0, 300.0}, {200.0, 100.0});
    REQUIRE(offset == LLUtils::PointF64{300.0, 250.0});

    const LLUtils::PointF64 clientPoint =
        OIV::ViewTransformController::ImageToClient(LLUtils::PointF64{10.0, 20.0}, 2.0, {5.0, 7.0});
    REQUIRE(clientPoint == LLUtils::PointF64{25.0, 47.0});
    REQUIRE(OIV::ViewTransformController::ClientToImage(clientPoint, 2.0, {5.0, 7.0}) ==
            LLUtils::PointF64{10.0, 20.0});
}

TEST_CASE("ViewTransformController bounds offsets using configured margins", "[AppCore]")
{
    REQUIRE(OIV::ViewTransformController::ResolveOffset(
                {500.0, -500.0}, {100.0, 100.0}, {300.0, 300.0}, {0.75, 0.75}) ==
            LLUtils::PointF64{75.0, -275.0});

    REQUIRE(OIV::ViewTransformController::ResolveOffset(
                {-100.0, 100.0}, {300.0, 300.0}, {100.0, 100.0}, {0.75, 0.75}) ==
            LLUtils::PointF64{-75.0, 100.0});
}

TEST_CASE("FileSorter orders files by name and extension", "[AppCore][Shared]")
{
    OIV::FileSorter sorter;
    const std::wstring png = L"C:\\images\\b.png";
    const std::wstring jpg = L"C:\\images\\a.jpg";

    sorter.SetSortType(OIV::FileSorter::SortType::Name);
    REQUIRE(sorter(jpg, png));

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    REQUIRE(sorter(jpg, png));
}

TEST_CASE("FileList updates the current folder list from watcher events", "[AppCore]")
{
    const auto folder = (std::filesystem::temp_directory_path() / "oiv-file-list-test").wstring();
    const auto fileA = (std::filesystem::path(folder) / L"a.png").wstring();
    const auto fileB = (std::filesystem::path(folder) / L"b.png").wstring();
    const auto fileC = (std::filesystem::path(folder) / L"c.png").wstring();
    std::filesystem::create_directories(folder);

    ActiveFileProvider activeProvider;
    activeProvider.activeFile = fileA;

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    std::vector<std::pair<OIV::FileList::index_type, OIV::FileList::index_type>> indexChanges;

    OIV::FileList fileList(&activeProvider,
                           &watcher,
                           &sorter,
                           {L"png"},
                           L"png",
                           [&](auto current, auto previous) { indexChanges.push_back({current, previous}); });

    fileList.SetFolder(folder, {fileA, fileC});
    REQUIRE(fileList.GetSize() == 2);
    REQUIRE(fileList.GetCurrentItemName() == fileA);

    watcher.Raise(OIV::IFileWatcher::FileChangedOp::Add, folder, L"b.png");
    REQUIRE(fileList.GetSize() == 3);
    REQUIRE(fileList.GetElementNameFromIndex(1) == fileB);

    watcher.Raise(OIV::IFileWatcher::FileChangedOp::Remove, folder, L"b.png");
    REQUIRE(fileList.GetSize() == 2);
    REQUIRE(fileList.GetElementNameFromIndex(1) == fileC);
}

TEST_CASE("ImageLoadController classifies load results", "[AppCore]")
{
    REQUIRE(OIV::ImageLoadController::ClassifyLoadResult(ResultCode::RC_Success, CreateTestImage()) ==
            OIV::ImageLoadStatus::Loaded);
    REQUIRE(OIV::ImageLoadController::ClassifyLoadResult(ResultCode::RC_FileNotSupported, nullptr) ==
            OIV::ImageLoadStatus::UnsupportedFormat);
    REQUIRE(OIV::ImageLoadController::ClassifyLoadResult(ResultCode::RC_UknownError, nullptr) ==
            OIV::ImageLoadStatus::UnknownError);
    REQUIRE(OIV::ImageLoadController::ClassifyLoadResult(
                ResultCode::RC_Success,
                CreateTestImage(OIV::ImageLoadController::MaxSupportedDimension + 1, 1)) ==
            OIV::ImageLoadStatus::TooLarge);
}

TEST_CASE("ImageLoadController loads direct files through the injected loader", "[AppCore]")
{
    auto fakeLoader = std::make_unique<FakeImageFileLoader>();
    auto* fakeLoaderPtr = fakeLoader.get();
    OIV::ImageLoadController controller(std::move(fakeLoader));

    const auto result = controller.LoadFile(
        L"C:\\images\\..\\images\\a.png",
        IMCodec::PluginTraverseMode::AnyPlugin,
        OIV::ImageLoadContext{800, 600});

    REQUIRE(result.status == OIV::ImageLoadStatus::Loaded);
    REQUIRE(result.DecodeSucceeded());
    REQUIRE(result.image != nullptr);
    REQUIRE(fakeLoaderPtr->lastPath ==
            std::filesystem::path(L"C:\\images\\..\\images\\a.png").lexically_normal().wstring());
    REQUIRE(fakeLoaderPtr->lastTraverseMode == IMCodec::PluginTraverseMode::AnyPlugin);
    REQUIRE(fakeLoaderPtr->lastContext.canvasWidth == 800);
    REQUIRE(fakeLoaderPtr->lastContext.canvasHeight == 600);
}

TEST_CASE("FileSessionController owns folder navigation", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-navigation-test");
    const auto fileA = folder / L"a.png";
    const auto fileB = folder / L"b.png";
    const auto fileC = folder / L"c.png";
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    ActiveFileProvider activeProvider;
    activeProvider.activeFile = fileA.wstring();

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidency residency(std::make_unique<CountingResidencyProcessor>(), 1);

    OIV::FileSessionController controller(
        &activeProvider,
        &watcher,
        &sorter,
        {L"png"},
        L"png",
        residency,
        [](const std::wstring&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    controller.LoadFileInFolder(fileA.wstring());

    REQUIRE(controller.GetFileList().GetSize() == 3);
    REQUIRE(controller.IsCurrentFile(fileA.wstring()));

    REQUIRE(controller.JumpFiles(1));
    REQUIRE(controller.IsCurrentFile(fileB.wstring()));

    REQUIRE(controller.JumpFiles(OIV::FileList::IndexEnd));
    REQUIRE(controller.IsCurrentFile(fileC.wstring()));
}

TEST_CASE("FileSessionController applies folder-load residency completions", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-folder-load-test");
    const auto fileA = folder / L"a.png";
    const auto fileB = folder / L"b.png";

    ActiveFileProvider activeProvider;
    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidency residency(std::make_unique<CountingResidencyProcessor>(), 1);

    OIV::FileSessionController controller(
        &activeProvider,
        &watcher,
        &sorter,
        {L"png"},
        L"png",
        residency,
        [](const std::wstring&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    OIV::BrowseResidencyManager::FileListSnapshot snapshot{
        folder.wstring(),
        {fileA.wstring(), fileB.wstring()},
        OIV::BrowseResidencyManager::FileListSnapshot::IndexStart};

    REQUIRE(controller.OnFolderLoadResidencyReady(snapshot, fileB.wstring(), CreateTestImage()));
    REQUIRE(controller.GetFileList().GetSize() == 2);
    REQUIRE(controller.IsCurrentFile(fileB.wstring()));

    REQUIRE_FALSE(controller.OnFolderLoadResidencyReady(snapshot, fileA.wstring(), nullptr));
    REQUIRE(controller.IsCurrentFile(fileB.wstring()));
}

TEST_CASE("ImageLoadController routes folders to file session", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-image-load-folder-route-test");
    const auto fileA = folder / L"a.png";
    TouchFile(fileA);

    ActiveFileProvider activeProvider;
    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidency residency(std::make_unique<CountingResidencyProcessor>(), 1);

    OIV::FileSessionController fileSessionController(
        &activeProvider,
        &watcher,
        &sorter,
        {L"png"},
        L"png",
        residency,
        [](const std::wstring&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseResidencyManager::FileListSnapshot&, const std::wstring&, IMCodec::ImageSharedPtr) {});

    auto fakeLoader = std::make_unique<FakeImageFileLoader>();
    auto* fakeLoaderPtr = fakeLoader.get();
    OIV::ImageLoadController controller(std::move(fakeLoader), &fileSessionController);

    const auto folderResult =
        controller.LoadFileOrFolder(folder.wstring(), IMCodec::PluginTraverseMode::NoTraverse, {});

    REQUIRE(folderResult.status == OIV::ImageLoadStatus::FolderLoadQueued);
    REQUIRE(fakeLoaderPtr->lastPath.empty());

    const auto emptyFolder = MakeTempFolder("oiv-image-load-empty-folder-route-test");
    const auto emptyFolderResult =
        controller.LoadFileOrFolder(emptyFolder.wstring(), IMCodec::PluginTraverseMode::NoTraverse, {});

    REQUIRE(emptyFolderResult.status == OIV::ImageLoadStatus::NoSupportedFiles);
}
