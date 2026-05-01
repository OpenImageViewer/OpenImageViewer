#include <catch2/catch_all.hpp>

#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/ColorCountPolicy.h>
#include <OIVAppCore/CommandManager.h>
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FileList.h>
#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVAppCore/FileRemovalPolicy.h>
#include <OIVAppCore/FileSessionController.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/ImageLoadController.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/IFileWatcher.h>
#include <OIVAppCore/SelectionRect.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SlideshowPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/ViewTransformController.h>
#include <OIVShared/AdaptiveMotion.h>
#include <OIVShared/FileSorter.h>
#include <OIVShared/ImageResidency.h>
#include <OIVShared/RecursiveDelayOp.h>

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

    IMCodec::PluginProperties CreatePlugin(IMCodec::CodecCapabilities capabilities,
                                           const std::wstring& description,
                                           std::initializer_list<std::wstring> extensions)
    {
        IMCodec::PluginProperties plugin;
        plugin.capabilities = capabilities;
        plugin.extensionCollection.push_back({description, extensions});
        return plugin;
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

TEST_CASE("ViewCommandPolicy parses view command arguments", "[AppCore]")
{
    const auto zoom = OIV::ViewCommandPolicy::ParseZoom(
        OIV::CommandManager::CommandArgs::FromString("val=1.25;cx=10;cy=20"));
    REQUIRE(zoom.amount == Catch::Approx(1.25));
    REQUIRE(zoom.centerX == 10);
    REQUIRE(zoom.centerY == 20);

    const auto centeredZoom =
        OIV::ViewCommandPolicy::ParseZoom(OIV::CommandManager::CommandArgs::FromString("val=0.5"));
    REQUIRE(centeredZoom.centerX == -1);
    REQUIRE(centeredZoom.centerY == -1);

    const auto pan =
        OIV::ViewCommandPolicy::ParsePan(OIV::CommandManager::CommandArgs::FromString("direction=left;amount=32"));
    REQUIRE(pan.direction == OIV::PanDirection::Left);
    REQUIRE(pan.amount == Catch::Approx(32.0));

    REQUIRE(OIV::ViewCommandPolicy::ParsePlacement(
                OIV::CommandManager::CommandArgs::FromString("cmd=fitToScreen")) ==
            OIV::PlacementAction::FitToScreen);
    REQUIRE(OIV::ViewCommandPolicy::ParsePlacement(
                OIV::CommandManager::CommandArgs::FromString("cmd=unknown")) == OIV::PlacementAction::None);

    REQUIRE(OIV::ViewCommandPolicy::FormatZoomResult(1.25) ==
            L"<textcolor=#ff8930>Zoom <textcolor=#7672ff>(125.00%)");
    REQUIRE(OIV::ViewCommandPolicy::FormatPlacementResult("Center") == L"<textcolor=#00ff00>Center");
}

TEST_CASE("ViewCommandPolicy parses navigation and window size decisions", "[AppCore]")
{
    auto navigation =
        OIV::ViewCommandPolicy::ParseNavigation(OIV::CommandManager::CommandArgs::FromString("amount=end;subimage=false"));
    REQUIRE(navigation.amount == OIV::FileList::IndexEnd);
    REQUIRE_FALSE(navigation.subImage);

    navigation =
        OIV::ViewCommandPolicy::ParseNavigation(OIV::CommandManager::CommandArgs::FromString("amount=-1;subimage=true"));
    REQUIRE(navigation.amount == -1);
    REQUIRE(navigation.subImage);
    REQUIRE(OIV::ViewCommandPolicy::NextSubImageIndex(0, -1, 3) == 2);
    REQUIRE(OIV::ViewCommandPolicy::NextSubImageIndex(2, 1, 3) == 0);

    auto window = OIV::ViewCommandPolicy::DecideWindowSize(
        OIV::CommandManager::CommandArgs::FromString("size_type=relative;width=50;height=25"),
        {400, 200},
        {300, 100},
        {0, 0, 1000, 800});
    REQUIRE(window.mode == OIV::WindowSizeMode::Windowed);
    REQUIRE(window.size == LLUtils::PointI32{500, 200});
    REQUIRE(window.position == LLUtils::PointI32{250, 100});

    window = OIV::ViewCommandPolicy::DecideWindowSize(
        OIV::CommandManager::CommandArgs::FromString("size_type=absolute;width=1200;height=900"),
        {400, 200},
        {900, 700},
        {0, 0, 1000, 800});
    REQUIRE(window.size == LLUtils::PointI32{1000, 800});
    REQUIRE(window.position == LLUtils::PointI32{0, 0});

    REQUIRE(OIV::ViewCommandPolicy::DecideWindowSize(
                OIV::CommandManager::CommandArgs::FromString("size_type=fullscreen"),
                {},
                {},
                {}).mode == OIV::WindowSizeMode::Fullscreen);
}

TEST_CASE("ImageTransformCommandPolicy parses and formats axis-aligned transforms", "[AppCore]")
{
    auto command = OIV::ImageTransformCommandPolicy::ParseAxisAlignedTransform(
        OIV::CommandManager::CommandArgs::FromString("type=rotatecw"));
    REQUIRE(command.HasTransform());
    REQUIRE(command.rotation == IMUtil::AxisAlignedRotation::Rotate90CW);
    REQUIRE(command.flip == IMUtil::AxisAlignedFlip::None);

    command = OIV::ImageTransformCommandPolicy::ParseAxisAlignedTransform(
        OIV::CommandManager::CommandArgs::FromString("type=hflip"));
    REQUIRE(command.HasTransform());
    REQUIRE(command.rotation == IMUtil::AxisAlignedRotation::None);
    REQUIRE(command.flip == IMUtil::AxisAlignedFlip::Horizontal);

    command = OIV::ImageTransformCommandPolicy::ParseAxisAlignedTransform(
        OIV::CommandManager::CommandArgs::FromString("type=unknown"));
    REQUIRE_FALSE(command.HasTransform());

    REQUIRE(OIV::ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                IMUtil::AxisAlignedRotation::Rotate180,
                IMUtil::AxisAlignedFlip::Vertical) ==
            L"Rotation <textcolor=#7672ff>(180 degrees)\n<textcolor=#ff8930>Flip <textcolor=#7672ff>(vertical)");
    REQUIRE(OIV::ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                IMUtil::AxisAlignedRotation::None,
                IMUtil::AxisAlignedFlip::None) == L"No transformation");
}

TEST_CASE("ViewerPresentationPolicy formats viewer messages", "[AppCore]")
{
    REQUIRE(OIV::ViewerPresentationPolicy::FormatOperationResult(OIV::OperationResult::NoDataFound) ==
            L"No Image loaded");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatFailedOperation(L"Cannot crop selected area",
                                                                  OIV::OperationResult::NoSelection) ==
            L"Cannot crop selected area - No selection");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatOpenedFileMessage(L"<path>") == L"File: <path>");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatTopMostMessage(2) == L"Top most ending in...2");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatNonFileTitlePrefix(OIV::ImageSource::Clipboard) ==
            L"Clipboard image - ");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatFileTitlePrefix(L"image",
                                                                  L".png",
                                                                  L"C:\\folder\\",
                                                                  true,
                                                                  3,
                                                                  10) ==
            L"3/10 | image.png @ C:\\folder - ");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatTitle(L"image - ", L"OpenImageViewer") ==
            L"image - OpenImageViewer");
}

TEST_CASE("SubImagePolicy maps visible sub-images and selections", "[AppCore]")
{
    REQUIRE(OIV::SubImagePolicy::IsVisible(IMCodec::ImageItemType::Image, 2));
    REQUIRE_FALSE(OIV::SubImagePolicy::IsVisible(IMCodec::ImageItemType::AnimationFrame, 2));
    REQUIRE_FALSE(OIV::SubImagePolicy::IsVisible(IMCodec::ImageItemType::Image, 0));

    REQUIRE(OIV::SubImagePolicy::IncludeMainImage(IMCodec::ImageItemType::Image));
    REQUIRE_FALSE(OIV::SubImagePolicy::IncludeMainImage(IMCodec::ImageItemType::Container));
    REQUIRE(OIV::SubImagePolicy::TotalDisplayedImages(3, true) == 4);
    REQUIRE(OIV::SubImagePolicy::TotalDisplayedImages(3, false) == 3);

    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(0, true) ==
            OIV::SubImagePolicy::MainImageIndex);
    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(2, true) == 1);
    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(2, false) == 2);

    REQUIRE(OIV::SubImagePolicy::InitialSelectionIndex(true, 500, {100, 200}) ==
            OIV::SubImagePolicy::MainImageIndex);
    REQUIRE(OIV::SubImagePolicy::InitialSelectionIndex(true, 100, {500, 200}) == 1);
    REQUIRE(OIV::SubImagePolicy::InitialSelectionIndex(false, 0, {100, 500}) == 1);
}

TEST_CASE("FrameLimiterPolicy decides refresh timing", "[AppCore]")
{
    REQUIRE(OIV::FrameLimiterPolicy::FrameWindowMicroseconds(60'000) == 16'666);

    auto decision = OIV::FrameLimiterPolicy::Decide(false, false, 0, 60'000);
    REQUIRE(decision.action == OIV::FrameRefreshAction::RefreshNow);

    decision = OIV::FrameLimiterPolicy::Decide(true, false, 20'000, 60'000);
    REQUIRE(decision.action == OIV::FrameRefreshAction::RefreshNow);

    decision = OIV::FrameLimiterPolicy::Decide(true, false, 10'000, 60'000);
    REQUIRE(decision.action == OIV::FrameRefreshAction::ScheduleRefresh);
    REQUIRE(decision.delayMs == 6);

    decision = OIV::FrameLimiterPolicy::Decide(true, true, 10'000, 60'000);
    REQUIRE(decision.action == OIV::FrameRefreshAction::None);
}

TEST_CASE("ImageEditPolicy validates selection operations", "[AppCore]")
{
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(false, true) ==
            OIV::OperationResult::NoDataFound);
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(true, true) ==
            OIV::OperationResult::NoSelection);
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(true, false) ==
            OIV::OperationResult::Success);
}

TEST_CASE("SortCommandPolicy decides sort type and direction changes", "[AppCore]")
{
    auto decision = OIV::SortCommandPolicy::Decide(
        OIV::CommandManager::CommandArgs::FromString("type=date"),
        OIV::FileSorter::SortType::Name);
    REQUIRE(decision.valid);
    REQUIRE(decision.sortType == OIV::FileSorter::SortType::Date);
    REQUIRE_FALSE(decision.reverseDirection);

    decision = OIV::SortCommandPolicy::Decide(
        OIV::CommandManager::CommandArgs::FromString("type=name"),
        OIV::FileSorter::SortType::Name);
    REQUIRE(decision.valid);
    REQUIRE(decision.reverseDirection);
    REQUIRE(OIV::SortCommandPolicy::Reverse(OIV::FileSorter::SortDirection::Ascending) ==
            OIV::FileSorter::SortDirection::Descending);
    REQUIRE(OIV::SortCommandPolicy::FormatSortResult("Sort by name", OIV::FileSorter::SortDirection::Ascending) ==
            L"Sort by name [Ascending]");
}

TEST_CASE("ColorCorrectionCommandPolicy parses and applies color corrections", "[AppCore]")
{
    const auto command = OIV::ColorCorrectionCommandPolicy::Parse(
        OIV::CommandManager::CommandArgs::FromString("type=gamma;op=increase;val=25"));
    REQUIRE(command.IsValid());
    REQUIRE(command.channel == OIV::ColorCorrectionChannel::Gamma);
    REQUIRE(command.value == Catch::Approx(25.0));

    REQUIRE(OIV::ColorCorrectionCommandPolicy::Apply(2.0, "increase", 25.0) == Catch::Approx(2.5));
    REQUIRE(OIV::ColorCorrectionCommandPolicy::Apply(2.0, "decrease", 100.0) == Catch::Approx(1.0));
    REQUIRE(OIV::ColorCorrectionCommandPolicy::Apply(2.0, "add", 3.0) == Catch::Approx(5.0));
    REQUIRE(OIV::ColorCorrectionCommandPolicy::Apply(2.0, "subtract", 0.5) == Catch::Approx(1.5));
    REQUIRE(OIV::ColorCorrectionCommandPolicy::FormatResult(command, 1.25) ==
            L"<textcolor=#00ff00>gamma<textcolor=#7672ff> increase 25%<textcolor=#00ff00> (125%)");
}

TEST_CASE("ColorCountPolicy ignores stale completions unless image info is visible", "[AppCore]")
{
    int currentImage = 0;
    int staleImage = 0;

    REQUIRE(OIV::ColorCountPolicy::DecideCompletion(&currentImage, &currentImage, false) ==
            OIV::ColorCountCompletionAction::ApplyToCurrentImage);
    REQUIRE(OIV::ColorCountPolicy::DecideCompletion(&staleImage, &currentImage, true) ==
            OIV::ColorCountCompletionAction::CountVisibleImage);
    REQUIRE(OIV::ColorCountPolicy::DecideCompletion(&staleImage, &currentImage, false) ==
            OIV::ColorCountCompletionAction::Ignore);

    REQUIRE(OIV::ColorCountPolicy::NormalizeCountResult(4, -1, -2) == 4);
    REQUIRE(OIV::ColorCountPolicy::NormalizeCountResult(-1, -1, -2) == -2);
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

TEST_CASE("SelectionWorkflowPolicy formats, places, and snaps selections", "[AppCore]")
{
    REQUIRE(OIV::SelectionWorkflowPolicy::FormatSelectionSize({{0, 0}, {12, 7}}) == L"12 X 7");

    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{20, 20}, {80, 60}}, {30, 10}, {200, 120}) ==
            LLUtils::PointI32{35, 10});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{20, 2}, {80, 60}}, {30, 10}, {200, 120}) ==
            LLUtils::PointI32{35, 60});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{170, 20}, {195, 60}}, {40, 10}, {200, 120}) ==
            LLUtils::PointI32{130, 40});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{2, 20}, {30, 60}}, {40, 10}, {200, 120}) ==
            LLUtils::PointI32{30, 40});

    REQUIRE(OIV::SelectionWorkflowPolicy::SnapToImagePixels({24, 33}, 2.0, {5.0, 7.0}) ==
            LLUtils::PointI32{25, 33});
    REQUIRE(OIV::SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(OIV::SelectionRect::Operation::Drag));
    REQUIRE_FALSE(
        OIV::SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(OIV::SelectionRect::Operation::NoOp));
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

TEST_CASE("ViewActionController resolves zoom and auto-place decisions", "[AppCore]")
{
    REQUIRE(OIV::ViewActionController::MinimumPixelSize(150.0, {1000.0, 300.0}) == Catch::Approx(0.5));
    REQUIRE(OIV::ViewActionController::MinimumPixelSize(150.0, {0.0, 300.0}) == Catch::Approx(1.0));

    REQUIRE(OIV::ViewActionController::ResolveZoomValue(50.0, false, 0.5, 30.0) == Catch::Approx(30.0));
    REQUIRE(OIV::ViewActionController::ResolveZoomValue(50.0, true, 0.5, 30.0) == Catch::Approx(50.0));

    REQUIRE(OIV::ViewActionController::ResolveZoomPoint({-1, 20}, {200.0, 100.0}) == LLUtils::PointI32{200, 20});
    REQUIRE(OIV::ViewActionController::RelativeZoom(2.0, 0.5) == Catch::Approx(3.0));
    REQUIRE(OIV::ViewActionController::RelativeZoom(2.0, -0.5) == Catch::Approx(2.0 / 1.5));

    REQUIRE(OIV::ViewActionController::ShouldPreserveOffsetLockForZoom(-1, -1));
    REQUIRE_FALSE(OIV::ViewActionController::ShouldPreserveOffsetLockForZoom(10, -1));
    REQUIRE(OIV::ViewActionController::ShouldFitToScreenOnAutoPlace(true, true));
    REQUIRE_FALSE(OIV::ViewActionController::ShouldFitToScreenOnAutoPlace(true, false));
    REQUIRE(OIV::ViewActionController::ShouldCenterOnAutoPlace(false, true, false));
    REQUIRE(OIV::ViewActionController::ShouldCenterOnAutoPlace(true, false, true));
}

TEST_CASE("SlideshowPolicy maps state to timer interval and wrap decision", "[AppCore]")
{
    OIV::SlideshowPolicy policy;

    REQUIRE_FALSE(policy.IsEnabled());
    REQUIRE(policy.GetTimerIntervalMs() == 0);

    policy.SetIntervalMs(1500);
    policy.SetEnabled(true);

    REQUIRE(policy.IsEnabled());
    REQUIRE(policy.GetIntervalMs() == 1500);
    REQUIRE(policy.GetTimerIntervalMs() == 1500);
    REQUIRE(policy.ShouldWrap(2, 3));
    REQUIRE_FALSE(policy.ShouldWrap(1, 3));
    REQUIRE_FALSE(policy.ShouldWrap(0, 0));
}

TEST_CASE("SequencerPolicy handles speed and frame timing", "[AppCore]")
{
    const auto args = OIV::CommandManager::CommandArgs::FromString("cmd=changespeed;amount=25");
    REQUIRE(OIV::SequencerPolicy::IsChangeSpeedCommand(args));
    REQUIRE(OIV::SequencerPolicy::ParseSpeedChangePercent(args) == Catch::Approx(25.0));
    REQUIRE(OIV::SequencerPolicy::ApplySpeedChange(2.0, 25.0) == Catch::Approx(2.5));
    REQUIRE(OIV::SequencerPolicy::FormatSpeed(1.25) ==
            L"<textcolor=#ff8930>Animation speed<textcolor=#7672ff> (125.00%)");
    REQUIRE(OIV::SequencerPolicy::NextFrame(2, 3) == 0);
    REQUIRE(OIV::SequencerPolicy::NextFrame(2, 0) == 0);
    REQUIRE(OIV::SequencerPolicy::FrameIntervalMs(2, 1.0) == 5);
    REQUIRE(OIV::SequencerPolicy::FrameIntervalMs(20, 2.0) == 10);
}

TEST_CASE("InputGesturePolicy maps pan vectors to cursor hints", "[AppCore]")
{
    auto hint = OIV::InputGesturePolicy::CursorHintForPan(LLUtils::PointF64::Zero);
    REQUIRE(hint.sizeAll);

    hint = OIV::InputGesturePolicy::CursorHintForPan({1.0, 0.0});
    REQUIRE_FALSE(hint.sizeAll);
    REQUIRE(hint.directionIndex == 4);

    hint = OIV::InputGesturePolicy::CursorHintForPan({-1.0, 0.0});
    REQUIRE_FALSE(hint.sizeAll);
    REQUIRE(hint.directionIndex == 0);
}

TEST_CASE("FileRemovalPolicy chooses removal actions", "[AppCore]")
{
    REQUIRE(OIV::FileRemovalPolicy::Decide(L"a.png", L"b.png", {}, true, true, 2) ==
            OIV::RemovedFileAction::Ignore);
    REQUIRE(OIV::FileRemovalPolicy::Decide(L"a.png", L"a.png", L"a.png", false, true, 2) ==
            OIV::RemovedFileAction::KeepMissingCurrent);
    REQUIRE(OIV::FileRemovalPolicy::Decide(L"a.png", L"a.png", {}, true, false, 2) ==
            OIV::RemovedFileAction::KeepMissingCurrent);
    REQUIRE(OIV::FileRemovalPolicy::Decide(L"a.png", L"a.png", L"a.png", true, true, 1) ==
            OIV::RemovedFileAction::TryStart);
    REQUIRE(OIV::FileRemovalPolicy::Decide(L"a.png", L"a.png", L"a.png", true, true, 3) ==
            OIV::RemovedFileAction::TryNextThenPrevious);
}

TEST_CASE("FileRemovalPolicy unloads only after failed removal jumps", "[AppCore]")
{
    REQUIRE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryStart, false, false));
    REQUIRE_FALSE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryStart, true, false));
    REQUIRE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(
        OIV::RemovedFileAction::TryNextThenPrevious,
        false,
        false));
    REQUIRE_FALSE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(
        OIV::RemovedFileAction::TryNextThenPrevious,
        false,
        true));
}

TEST_CASE("FileChangePolicy routes watcher events", "[AppCore]")
{
    using Op = OIV::IFileWatcher::FileChangedOp;

    OIV::IFileWatcher::FileChangedEventArgs eventArgs{7, Op::Modified, L"C:\\images", L"a.png", {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, L"C:\\images\\a.png") ==
            OIV::FileChangeAction::CurrentFileChanged);
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, L"C:\\images\\b.png") ==
            OIV::FileChangeAction::Ignore);

    eventArgs = {7, Op::Rename, L"C:\\images", L"old.png", L"new.png"};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, L"C:\\images\\new.png") ==
            OIV::FileChangeAction::CurrentFileChanged);

    eventArgs = {7, Op::WatchedFolderRemoved, L"C:\\images", {}, {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) ==
            OIV::FileChangeAction::ClearWatchedFolder);

    eventArgs = {9, Op::Modified, L"C:\\config", L"Settings.json", {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) == OIV::FileChangeAction::ReloadSettings);

    eventArgs = {11, Op::Modified, L"C:\\other", L"x.png", {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) == OIV::FileChangeAction::UnexpectedFolder);
}

TEST_CASE("FileReloadPolicy decides reload timing", "[AppCore]")
{
    OIV::FileReloadPolicy policy;

    policy.SetMode(OIV::FileReloadMode::None);
    REQUIRE(policy.OnCurrentFileChanged(L"a.png", true) == OIV::ReloadAction::None);

    policy.SetMode(OIV::FileReloadMode::AutoBackground);
    REQUIRE(policy.OnCurrentFileChanged(L"a.png", false) == OIV::ReloadAction::RequestNow);

    policy.SetMode(OIV::FileReloadMode::AutoForeground);
    REQUIRE(policy.OnCurrentFileChanged(L"a.png", true) == OIV::ReloadAction::RequestNow);
    REQUIRE(policy.OnCurrentFileChanged(L"a.png", false) == OIV::ReloadAction::Defer);
    REQUIRE(policy.HasPendingReloadFor(L"a.png"));
    REQUIRE(policy.OnPendingReloadRequested(L"a.png") == OIV::ReloadAction::RequestNow);
    REQUIRE_FALSE(policy.HasPendingReloadFor(L"a.png"));
}

TEST_CASE("FileReloadPolicy owns confirmation pending state", "[AppCore]")
{
    OIV::FileReloadPolicy policy;
    policy.SetMode(OIV::FileReloadMode::Confirmation);

    REQUIRE(policy.OnCurrentFileChanged(L"a.png", false) == OIV::ReloadAction::Defer);
    REQUIRE(policy.GetPendingReloadFile() == L"a.png");
    REQUIRE(policy.OnPendingReloadRequested(L"a.png") == OIV::ReloadAction::AskUser);
    REQUIRE(policy.ConfirmReload(false) == OIV::ReloadAction::None);
    REQUIRE(policy.GetPendingReloadFile().empty());

    REQUIRE(policy.OnCurrentFileChanged(L"b.png", true) == OIV::ReloadAction::AskUser);
    REQUIRE(policy.ConfirmReload(true) == OIV::ReloadAction::RequestNow);
    REQUIRE(policy.GetPendingReloadFile().empty());
}

TEST_CASE("AppSettingsPolicy parses typed settings values", "[AppCore]")
{
    REQUIRE(OIV::AppSettingsPolicy::ParseIntegral(L"42") == 42);
    REQUIRE(OIV::AppSettingsPolicy::ParseFloat(L"1.5") == Catch::Approx(1.5));
    REQUIRE(OIV::AppSettingsPolicy::ParseBool(L"true"));
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseBool(L"false"));

    auto deletedMode = OIV::AppSettingsPolicy::ParseDeletedFileRemovalMode(L"always");
    REQUIRE(deletedMode.valid);
    REQUIRE((deletedMode.value & OIV::DeletedFileRemovalMode::DeletedInternally) ==
            OIV::DeletedFileRemovalMode::DeletedInternally);
    REQUIRE((deletedMode.value & OIV::DeletedFileRemovalMode::DeletedExternally) ==
            OIV::DeletedFileRemovalMode::DeletedExternally);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseDeletedFileRemovalMode(L"bad").valid);

    auto reloadMode = OIV::AppSettingsPolicy::ParseFileReloadMode(L"autoforeground");
    REQUIRE(reloadMode.valid);
    REQUIRE(reloadMode.value == OIV::FileReloadMode::AutoForeground);

    auto sortType = OIV::AppSettingsPolicy::ParseSortType(L"extension");
    REQUIRE(sortType.valid);
    REQUIRE(sortType.value == OIV::FileSorter::SortType::Extension);
    REQUIRE(OIV::AppSettingsPolicy::ParseSortDirection(L"ascending") ==
            OIV::FileSorter::SortDirection::Ascending);
    REQUIRE(OIV::AppSettingsPolicy::ParseSortDirection(L"descending") ==
            OIV::FileSorter::SortDirection::Descending);

    auto sortDirectionTarget = OIV::AppSettingsPolicy::ParseSortDirectionTarget(L"files/sortbydatedirection");
    REQUIRE(sortDirectionTarget.valid);
    REQUIRE(sortDirectionTarget.value == OIV::FileSorter::SortType::Date);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseSortDirectionTarget(L"files/unknown").valid);

    auto backgroundColorIndex = OIV::AppSettingsPolicy::ParseBackgroundColorIndex(L"displaysettings/backgroundcolor2");
    REQUIRE(backgroundColorIndex.valid);
    REQUIRE(backgroundColorIndex.value == 1);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseBackgroundColorIndex(L"displaysettings/other").valid);
}

TEST_CASE("AppSettingsPolicy maps setting changes to typed actions", "[AppCore]")
{
    using ActionType = OIV::AppSettingsPolicy::ActionType;

    auto action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/maxzoom", L"12.5");
    REQUIRE(action.type == ActionType::MaxZoom);
    REQUIRE(action.floatValue == Catch::Approx(12.5));

    action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/imagemargins/x", L"0.25");
    REQUIRE(action.type == ActionType::ImageMarginX);
    REQUIRE(action.floatValue == Catch::Approx(0.25));

    action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/imagemargins/y", L"0.5");
    REQUIRE(action.type == ActionType::ImageMarginY);
    REQUIRE(action.floatValue == Catch::Approx(0.5));

    action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/minimagesize", L"128");
    REQUIRE(action.type == ActionType::MinImageSize);
    REQUIRE(action.floatValue == Catch::Approx(128.0));

    action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/slideshowinterval", L"1500");
    REQUIRE(action.type == ActionType::SlideshowInterval);
    REQUIRE(action.integralValue == 1500);

    action = OIV::AppSettingsPolicy::ParseAction(L"viewsettings/quickbrowsedelay", L"75");
    REQUIRE(action.type == ActionType::QuickBrowseDelay);
    REQUIRE(action.integralValue == 75);

    action = OIV::AppSettingsPolicy::ParseAction(L"autoscroll/deadzoneradius", L"40");
    REQUIRE(action.type == ActionType::AutoScrollDeadZoneRadius);
    REQUIRE(action.integralValue == 40);

    action = OIV::AppSettingsPolicy::ParseAction(L"autoscroll/speedfactorin", L"1.25");
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorIn);
    REQUIRE(action.floatValue == Catch::Approx(1.25));

    action = OIV::AppSettingsPolicy::ParseAction(L"autoscroll/speedfactorout", L"2.5");
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorOut);
    REQUIRE(action.floatValue == Catch::Approx(2.5));

    action = OIV::AppSettingsPolicy::ParseAction(L"autoscroll/speedfactorrange", L"6");
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorRange);
    REQUIRE(action.integralValue == 6);

    action = OIV::AppSettingsPolicy::ParseAction(L"autoscroll/maxspeed", L"120");
    REQUIRE(action.type == ActionType::AutoScrollMaxSpeed);
    REQUIRE(action.integralValue == 120);

    action = OIV::AppSettingsPolicy::ParseAction(L"filesystem/deletedfileremovalmode", L"externally");
    REQUIRE(action.type == ActionType::DeletedFileRemovalMode);
    REQUIRE(action.deletedFileRemovalMode == OIV::DeletedFileRemovalMode::DeletedExternally);

    action = OIV::AppSettingsPolicy::ParseAction(L"filesystem/modifiedfilereloadmode", L"autobackground");
    REQUIRE(action.type == ActionType::FileReloadMode);
    REQUIRE(action.fileReloadMode == OIV::FileReloadMode::AutoBackground);

    action = OIV::AppSettingsPolicy::ParseAction(L"system/reloadsettingsfileifchanged", L"true");
    REQUIRE(action.type == ActionType::ReloadSettingsFileIfChanged);
    REQUIRE(action.boolValue);

    action = OIV::AppSettingsPolicy::ParseAction(L"files/defaultsortmode", L"extension");
    REQUIRE(action.type == ActionType::DefaultSortMode);
    REQUIRE(action.sortType == OIV::FileSorter::SortType::Extension);

    action = OIV::AppSettingsPolicy::ParseAction(L"files/sortbydatedirection", L"descending");
    REQUIRE(action.type == ActionType::SortDirection);
    REQUIRE(action.sortType == OIV::FileSorter::SortType::Date);
    REQUIRE(action.sortDirection == OIV::FileSorter::SortDirection::Descending);

    action = OIV::AppSettingsPolicy::ParseAction(L"displaysettings/backgroundcolor2", L"#11223344");
    REQUIRE(action.type == ActionType::BackgroundColor);
    REQUIRE(action.backgroundColorIndex == 1);
    REQUIRE(action.textValue == L"#11223344");

    action = OIV::AppSettingsPolicy::ParseAction(L"imagesettings/biggestsubimage", L"false");
    REQUIRE(action.type == ActionType::BiggestSubImageOnLoad);
    REQUIRE_FALSE(action.boolValue);

    REQUIRE(OIV::AppSettingsPolicy::ParseAction(L"unknown/key", L"value").type == ActionType::None);
    REQUIRE(OIV::AppSettingsPolicy::ParseAction(L"filesystem/deletedfileremovalmode", L"bad").type ==
            ActionType::None);
    REQUIRE(OIV::AppSettingsPolicy::ParseAction(L"files/defaultsortmode", L"bad").type == ActionType::None);
}

TEST_CASE("ImageFormatCatalogPolicy builds image dialog filters", "[AppCore]")
{
    const auto catalog = OIV::ImageFormatCatalogPolicy::Build(
        {CreatePlugin(IMCodec::CodecCapabilities::Decode | IMCodec::CodecCapabilities::Encode,
                      L"PNG image",
                      {L"PNG", L"Apng"}),
         CreatePlugin(IMCodec::CodecCapabilities::Decode, L"JPEG image", {L"jpg", L"jpeg"}),
         CreatePlugin(IMCodec::CodecCapabilities::Encode | IMCodec::CodecCapabilities::BulkCodec,
                      L"Bulk writer",
                      {L"bulk"})});

    REQUIRE(catalog.readFilters.size() == 4);
    REQUIRE(catalog.readFilters[0].description == L"All files (*.*)");
    REQUIRE(catalog.readFilters[0].extensions == std::vector<std::wstring>{L"*.*"});
    REQUIRE(catalog.readFilters[1].description == L"All supported image formats");
    REQUIRE(catalog.readFilters[1].extensions ==
            std::vector<std::wstring>{L"*.png", L"*.apng", L"*.jpg", L"*.jpeg"});
    REQUIRE(catalog.readFilters[2].description == L"PNG/APNG - PNG image");
    REQUIRE(catalog.readFilters[2].extensions == std::vector<std::wstring>{L"*.png", L"*.apng"});

    REQUIRE(catalog.writeFilters.size() == 1);
    REQUIRE(catalog.writeFilters[0].description == L"PNG/APNG - PNG image");
    REQUIRE(catalog.writeFilters[0].extensions == std::vector<std::wstring>{L"*.png"});
    REQUIRE(catalog.defaultSaveFileFormatIndex == 1);
    REQUIRE(catalog.defaultSaveFileExtension == L"png");
    REQUIRE(catalog.knownFileTypesSet == std::set<std::wstring>{L"apng", L"jpeg", L"jpg", L"png"});
    REQUIRE(catalog.knownFileTypes == L"apng;jpeg;jpg;png");
}

TEST_CASE("ImageFormatCatalogPolicy defaults save filter index without PNG", "[AppCore]")
{
    const auto catalog = OIV::ImageFormatCatalogPolicy::Build(
        {CreatePlugin(IMCodec::CodecCapabilities::Encode, L"JPEG image", {L"jpg"})});

    REQUIRE(catalog.writeFilters.size() == 1);
    REQUIRE(catalog.writeFilters[0].extensions == std::vector<std::wstring>{L"*.jpg"});
    REQUIRE(catalog.defaultSaveFileFormatIndex == 0);
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

TEST_CASE("ImageLoadPresentationPolicy maps load results to UI decisions", "[AppCore]")
{
    OIV::ImageLoadResult result{OIV::ImageLoadStatus::FolderLoadQueued};
    auto presentation = OIV::ImageLoadPresentationPolicy::Decide(result, L"path.png");
    REQUIRE(presentation.succeeded);
    REQUIRE_FALSE(presentation.shouldLoadImage);
    REQUIRE_FALSE(presentation.shouldShowMessage);

    result = {OIV::ImageLoadStatus::Loaded, ResultCode::RC_Success, L"path.png", nullptr};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, L"path.png");
    REQUIRE(presentation.succeeded);
    REQUIRE(presentation.shouldLoadImage);

    result = {OIV::ImageLoadStatus::UnsupportedFormat, ResultCode::RC_FileNotSupported, L"path.png", nullptr};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, L"<path>");
    REQUIRE_FALSE(presentation.succeeded);
    REQUIRE(presentation.shouldShowMessage);
    REQUIRE(presentation.message == L"Can not load the file: <path>, image format is not supported");

    result = {OIV::ImageLoadStatus::NoSupportedFiles};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, L"path.png");
    REQUIRE_FALSE(presentation.succeeded);
    REQUIRE_FALSE(presentation.shouldShowMessage);
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
