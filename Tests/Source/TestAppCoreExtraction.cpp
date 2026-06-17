#include <LLUtils/StringDefs.h>
#include <catch2/catch_all.hpp>

#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/ColorCountPolicy.h>
#include <OIVAppCore/CommandManager.h>
#include <OIVAppCore/CommandController.h>
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FolderFileList.h>
#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVAppCore/FileRemovalPolicy.h>
#include <OIVAppCore/BrowseSessionController.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageInfoPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/ImageOpenController.h>
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
#include <OIVShared/ImageResidencyCache.h>
#include <OIVShared/RecursiveDelayOp.h>
#include <OIVShared/UnitFormatter.h>

#include <Image.h>
#include <IImageCodec.h>
#include <ImageItem.h>
#include <LLUtils/Event.h>
#include <LLUtils/StringUtility.h>
#include <OIVImage/OIVFileImage.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    class FakeFileWatcher : public OIV::IFileWatcher
    {
      public:

        bool IsFolderRegistered(const LLUtils::native_string_type& folder) const override { return fFolder == folder; }

        FolderID AddFolder(const LLUtils::native_string_type& folder) override
        {
            fFolder = folder;
            return fFolderID;
        }

        void RemoveFolder(FolderID folderID) override
        {
            if (folderID == fFolderID)
                fFolder.clear();
        }

        void RemoveFolder(const LLUtils::native_string_type& folder) override
        {
            if (folder == fFolder)
                fFolder.clear();
        }

        OnFileChangedEventArgsEvent& GetFileChangedEvent() override { return fEvent; }

        void Raise(FileChangedOp op, const LLUtils::native_string_type& folder,
                   const LLUtils::native_string_type& fileName, const LLUtils::native_string_type& fileName2 = {})
        {
            fEvent.Raise(FileChangedEventArgs{fFolderID, op, folder, fileName, fileName2});
        }

      private:

        FolderID fFolderID = 7;
        LLUtils::native_string_type fFolder;
        OnFileChangedEventArgsEvent fEvent;
    };

    IMCodec::ImageSharedPtr CreateTestImage(uint32_t width = 1, uint32_t height = 1)
    {
        auto imageItem               = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType          = IMCodec::ImageItemType::Image;
        imageItem->descriptor.width  = width;
        imageItem->descriptor.height = height;
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    IMCodec::ImageSharedPtr CreateFormattedTestImage(uint32_t width = 1, uint32_t height = 1,
                                                     IMCodec::TexelFormat format = IMCodec::TexelFormat::I_R8_G8_B8_A8)
    {
        auto imageItem                                = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType                           = IMCodec::ImageItemType::Image;
        imageItem->descriptor.width                   = width;
        imageItem->descriptor.height                  = height;
        imageItem->descriptor.texelFormatDecompressed = format;
        imageItem->descriptor.texelFormatStorage      = format;
        imageItem->descriptor.rowPitchInBytes         = width * IMCodec::GetTexelFormatSize(format) / CHAR_BIT;
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    class FakeImageCodec : public IMCodec::IImageCodec
    {
      public:

        IMCodec::ImageResult Decode(const std::byte*, std::size_t, const IMCodec::PluginID&, IMCodec::ImageLoadFlags,
                                    const IMCodec::Parameters&, IMCodec::ImageSharedPtr&) override
        {
            return IMCodec::ImageResult::NotImplemented;
        }

        IMCodec::ImageResult Encode(const IMCodec::ImageSharedPtr, const IMCodec::PluginID&, const IMCodec::Parameters&,
                                    LLUtils::Buffer&) override
        {
            return IMCodec::ImageResult::NotImplemented;
        }

        IMCodec::ImageResult GetEncoderParameters(const IMCodec::PluginID&, IMCodec::ListParameterDescriptors&) override
        {
            return IMCodec::ImageResult::NotImplemented;
        }

        IMCodec::ImageResult InstallPlugin(IMCodec::IImagePlugin*) override
        {
            return IMCodec::ImageResult::NotImplemented;
        }

        IMCodec::ImageResult InstallPlugin(const LLUtils::native_string_type&) override
        {
            return IMCodec::ImageResult::NotImplemented;
        }

        IMCodec::ImageResult GetPluginInfo(const IMCodec::PluginID&,
                                           IMCodec::PluginProperties& pluginProperties) override
        {
            pluginProperties.pluginDescription = pluginDescription;
            return pluginInfoResult;
        }

        std::vector<IMCodec::PluginProperties> GetPluginsInfo() override { return {}; }

        IMCodec::ImageResult pluginInfoResult         = IMCodec::ImageResult::Success;
        LLUtils::native_string_type pluginDescription = LLUTILS_TEXT("Fake codec");
    };

    const OIV::ImageInfoPresentationPolicy::ImageInfoRow* FindRow(
        const OIV::ImageInfoPresentationPolicy::ImageInfoRows& rows, const std::string& key)
    {
        auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.key == key; });
        return it == rows.end() ? nullptr : &*it;
    }

    class CountingResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey&,
                                     OIV::ImageResidencyCacheValue& outValue) override
        {
            ++fCallCount;
            outValue = CreateTestImage();
            return true;
        }

        int GetCallCount() const { return fCallCount; }

      private:

        std::atomic<int> fCallCount = 0;
    };

    class SelectiveResidencyProcessor : public OIV::RequestProcessorType
    {
      public:

        explicit SelectiveResidencyProcessor(std::set<LLUtils::native_string_type> successfulFiles)
            : fSuccessfulFiles(std::move(successfulFiles))
        {
        }

        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey& key,
                                     OIV::ImageResidencyCacheValue& outValue) override
        {
            ++fCallCount;
            const auto& fileName = std::get<0>(key);
            if (fSuccessfulFiles.contains(fileName) == false)
                return false;

            outValue = CreateTestImage();
            return true;
        }

        int GetCallCount() const { return fCallCount; }

      private:

        std::set<LLUtils::native_string_type> fSuccessfulFiles;
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
                                           const LLUtils::native_string_type& description,
                                           std::initializer_list<LLUtils::native_string_type> extensions)
    {
        IMCodec::PluginProperties plugin;
        plugin.capabilities = capabilities;
        plugin.extensionCollection.push_back({description, extensions});
        return plugin;
    }

    class FakeImageFileLoader : public OIV::IImageFileLoader
    {
      public:

        OIV::ImageFileLoadResult LoadFile(const LLUtils::native_string_type& normalizedFilePath,
                                          IMCodec::PluginTraverseMode traverseMode,
                                          const OIV::ImageLoadContext& context) override
        {
            lastPath         = normalizedFilePath;
            lastTraverseMode = traverseMode;
            lastContext      = context;

            return OIV::ImageFileLoadResult{resultCode,
                                            image != nullptr
                                                ? std::make_shared<OIV::OIVFileImage>(normalizedFilePath, image)
                                                : nullptr};
        }

        ResultCode resultCode         = ResultCode::RC_Success;
        IMCodec::ImageSharedPtr image = CreateTestImage();
        LLUtils::native_string_type lastPath;
        IMCodec::PluginTraverseMode lastTraverseMode = IMCodec::PluginTraverseMode::NoTraverse;
        OIV::ImageLoadContext lastContext;
    };
}  // namespace

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

TEST_CASE("UnitFormatter formats binary and decimal units", "[Shared]")
{
    REQUIRE(OIV::UnitFormatter::FormatUnit(2048, OIV::UnitType::BinaryDataShort, 1, 0) == LLUTILS_TEXT("2.0KB"));
    REQUIRE(OIV::UnitFormatter::FormatUnit(1500, OIV::UnitType::Distance, 2, 0) == LLUTILS_TEXT("1.50 meters"));
}

TEST_CASE("CommandManager dispatches predefined command groups", "[AppCore]")
{
    OIV::CommandManager manager;
    bool executed = false;

    manager.AddCommand(OIV::CommandManager::Command(
        "open",
        [&](const OIV::CommandManager::CommandRequest& request, OIV::CommandManager::CommandResult& result)
        {
            executed        = true;
            result.resValue = LLUtils::StringUtility::ToNativeString(request.args.GetArgValue("path"));
        }));

    manager.AddCommandGroup({"OpenImage", "Open image", "open", "path=image.png"});

    OIV::CommandManager::CommandResult result;
    REQUIRE(manager.ExecuteCommand(manager.GetCommandRequestGroup("OpenImage"), result));
    REQUIRE(executed);
    REQUIRE(result.resValue == LLUTILS_TEXT("image.png"));
}

TEST_CASE("CommandController dispatches commands and forwards results", "[AppCore]")
{
    LLUtils::native_string_type forwardedResult;
    OIV::CommandController controller([&](const LLUtils::native_string_type& result) { forwardedResult = result; });

    controller.AddCommandCallbacks(
        {{"open", [](const OIV::CommandManager::CommandRequest& request, OIV::CommandManager::CommandResult& result)
          { result.resValue = LLUtils::StringUtility::ToNativeString(request.args.GetArgValue("path")); }}});

    controller.GetCommandManager().AddCommandGroup({"OpenImage", "Open image", "open", "path=image.png"});

    REQUIRE(controller.ExecutePredefinedCommand("OpenImage"));
    REQUIRE(forwardedResult == LLUTILS_TEXT("image.png"));
    REQUIRE_FALSE(controller.ExecutePredefinedCommand("MissingCommand"));
}

TEST_CASE("ViewCommandPolicy parses view command arguments", "[AppCore]")
{
    const auto zoom = OIV::ViewCommandPolicy::ParseZoom(
        OIV::CommandManager::CommandArgs::FromString("val=1.25;cx=10;cy=20"));
    REQUIRE(zoom.amount == Catch::Approx(1.25));
    REQUIRE(zoom.centerX == 10);
    REQUIRE(zoom.centerY == 20);

    const auto centeredZoom = OIV::ViewCommandPolicy::ParseZoom(
        OIV::CommandManager::CommandArgs::FromString("val=0.5"));
    REQUIRE(centeredZoom.centerX == -1);
    REQUIRE(centeredZoom.centerY == -1);

    const auto pan = OIV::ViewCommandPolicy::ParsePan(
        OIV::CommandManager::CommandArgs::FromString("direction=left;amount=32"));
    REQUIRE(pan.direction == OIV::PanDirection::Left);
    REQUIRE(pan.amount == Catch::Approx(32.0));

    REQUIRE(OIV::ViewCommandPolicy::ParsePlacement(OIV::CommandManager::CommandArgs::FromString("cmd=fitToScreen")) ==
            OIV::PlacementAction::FitToScreen);
    REQUIRE(OIV::ViewCommandPolicy::ParsePlacement(OIV::CommandManager::CommandArgs::FromString("cmd=unknown")) ==
            OIV::PlacementAction::None);

    REQUIRE(OIV::ViewCommandPolicy::FormatZoomResult(1.25) ==
            LLUTILS_TEXT("<textcolor=#ff8930>Zoom <textcolor=#7672ff>(125.00%)"));
    REQUIRE(OIV::ViewCommandPolicy::FormatPlacementResult("Center") == LLUTILS_TEXT("<textcolor=#00ff00>Center"));
}

TEST_CASE("ViewCommandPolicy parses navigation and window size decisions", "[AppCore]")
{
    auto navigation = OIV::ViewCommandPolicy::ParseNavigation(
        OIV::CommandManager::CommandArgs::FromString("amount=end;subimage=false"));
    REQUIRE(navigation.amount == OIV::FolderFileList::IndexEnd);
    REQUIRE_FALSE(navigation.subImage);

    navigation = OIV::ViewCommandPolicy::ParseNavigation(
        OIV::CommandManager::CommandArgs::FromString("amount=-1;subimage=true"));
    REQUIRE(navigation.amount == -1);
    REQUIRE(navigation.subImage);
    REQUIRE(OIV::ViewCommandPolicy::NextSubImageIndex(0, -1, 3) == 2);
    REQUIRE(OIV::ViewCommandPolicy::NextSubImageIndex(2, 1, 3) == 0);

    auto window = OIV::ViewCommandPolicy::DecideWindowSize(OIV::CommandManager::CommandArgs::FromString(
                                                               "size_type=relative;width=50;height=25"),
                                                           {400, 200}, {300, 100}, {0, 0, 1000, 800});
    REQUIRE(window.mode == OIV::WindowSizeMode::Windowed);
    REQUIRE(window.size == LLUtils::PointI32{500, 200});
    REQUIRE(window.position == LLUtils::PointI32{250, 100});

    window = OIV::ViewCommandPolicy::DecideWindowSize(OIV::CommandManager::CommandArgs::FromString(
                                                          "size_type=absolute;width=1200;height=900"),
                                                      {400, 200}, {900, 700}, {0, 0, 1000, 800});
    REQUIRE(window.size == LLUtils::PointI32{1000, 800});
    REQUIRE(window.position == LLUtils::PointI32{0, 0});

    REQUIRE(OIV::ViewCommandPolicy::DecideWindowSize(
                OIV::CommandManager::CommandArgs::FromString("size_type=fullscreen"), {}, {}, {})
                .mode == OIV::WindowSizeMode::Fullscreen);
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

    REQUIRE(OIV::ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(IMUtil::AxisAlignedRotation::Rotate180,
                                                                               IMUtil::AxisAlignedFlip::Vertical) ==
            LLUTILS_TEXT(
                "Rotation <textcolor=#7672ff>(180 degrees)\n<textcolor=#ff8930>Flip <textcolor=#7672ff>(vertical)"));
    REQUIRE(OIV::ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::None) == LLUTILS_TEXT("No transformation"));
}

TEST_CASE("ViewerPresentationPolicy formats viewer messages", "[AppCore]")
{
    REQUIRE(OIV::ViewerPresentationPolicy::FormatOperationResult(OIV::OperationResult::NoDataFound) ==
            LLUTILS_TEXT("No Image loaded"));
    REQUIRE(OIV::ViewerPresentationPolicy::FormatFailedOperation(LLUTILS_TEXT("Cannot crop selected area"),
                                                                 OIV::OperationResult::NoSelection) ==
            LLUTILS_TEXT("Cannot crop selected area - No selection"));
    REQUIRE(OIV::ViewerPresentationPolicy::FormatOpenedFileMessage(LLUTILS_TEXT("<path>")) ==
            LLUTILS_TEXT("File: <path>"));
    REQUIRE(OIV::ViewerPresentationPolicy::FormatTopMostMessage(2) == LLUTILS_TEXT("Top most ending in...2"));
    REQUIRE(OIV::ViewerPresentationPolicy::FormatNonFileTitlePrefix(OIV::ImageSource::Clipboard) ==
            LLUTILS_TEXT("Clipboard image - "));
    const auto titleFolder             = std::filesystem::path{} / "folder";
    const auto expectedFileTitlePrefix = LLUTILS_TEXT("3/10 | image.png @ ") + titleFolder.native() +
                                         LLUTILS_TEXT(" - ");
    REQUIRE(OIV::ViewerPresentationPolicy::FormatFileTitlePrefix(LLUTILS_TEXT("image"), LLUTILS_TEXT(".png"),
                                                                 titleFolder.native(), true, 3,
                                                                 10) == expectedFileTitlePrefix);
    REQUIRE(OIV::ViewerPresentationPolicy::FormatFileTitlePrefix(LLUTILS_TEXT("image"), LLUTILS_TEXT(".png"),
                                                                 titleFolder.native() + std::filesystem::path::preferred_separator, true,
                                                                 3, 10) == expectedFileTitlePrefix);
    REQUIRE(OIV::ViewerPresentationPolicy::FormatTitle(LLUTILS_TEXT("image - "), LLUTILS_TEXT("OpenImageViewer")) ==
            LLUTILS_TEXT("image - OpenImageViewer"));
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

    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(0, true) == OIV::SubImagePolicy::MainImageIndex);
    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(2, true) == 1);
    REQUIRE(OIV::SubImagePolicy::ActualImageIndexFromDisplayIndex(2, false) == 2);

    REQUIRE(OIV::SubImagePolicy::InitialSelectionIndex(true, 500, {100, 200}) == OIV::SubImagePolicy::MainImageIndex);
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
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(false, true) == OIV::OperationResult::NoDataFound);
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(true, true) == OIV::OperationResult::NoSelection);
    REQUIRE(OIV::ImageEditPolicy::ValidateSelectionOperation(true, false) == OIV::OperationResult::Success);
}

TEST_CASE("SortCommandPolicy decides sort type and direction changes", "[AppCore]")
{
    auto decision = OIV::SortCommandPolicy::Decide(OIV::CommandManager::CommandArgs::FromString("type=date"),
                                                   OIV::FileSorter::SortType::Name);
    REQUIRE(decision.valid);
    REQUIRE(decision.sortType == OIV::FileSorter::SortType::Date);
    REQUIRE_FALSE(decision.reverseDirection);

    decision = OIV::SortCommandPolicy::Decide(OIV::CommandManager::CommandArgs::FromString("type=name"),
                                              OIV::FileSorter::SortType::Name);
    REQUIRE(decision.valid);
    REQUIRE(decision.reverseDirection);
    REQUIRE(OIV::SortCommandPolicy::Reverse(OIV::FileSorter::SortDirection::Ascending) ==
            OIV::FileSorter::SortDirection::Descending);
    REQUIRE(OIV::SortCommandPolicy::FormatSortResult("Sort by name", OIV::FileSorter::SortDirection::Ascending) ==
            LLUTILS_TEXT("Sort by name [Ascending]"));
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
            LLUTILS_TEXT("<textcolor=#00ff00>gamma<textcolor=#7672ff> increase 25%<textcolor=#00ff00> (125%)"));
}

TEST_CASE("ColorCountPolicy ignores stale completions unless image info is visible", "[AppCore]")
{
    int currentImage = 0;
    int staleImage   = 0;

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
    OIV::SelectionRect selection([&](const LLUtils::RectI32& rect, bool visible)
                                 { changes.push_back({rect, visible}); });

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
    REQUIRE(OIV::SelectionWorkflowPolicy::FormatSelectionSize({{0, 0}, {12, 7}}) == LLUTILS_TEXT("12 X 7"));

    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{20, 20}, {80, 60}}, {30, 10}, {200, 120}) ==
            LLUtils::PointI32{35, 10});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{20, 2}, {80, 60}}, {30, 10}, {200, 120}) ==
            LLUtils::PointI32{35, 60});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{170, 20}, {195, 60}}, {40, 10}, {200, 120}) ==
            LLUtils::PointI32{130, 40});
    REQUIRE(OIV::SelectionWorkflowPolicy::PlaceSelectionLabel({{2, 20}, {30, 60}}, {40, 10}, {200, 120}) ==
            LLUtils::PointI32{30, 40});

    REQUIRE(OIV::SelectionWorkflowPolicy::SnapToImagePixels({24, 33}, 2.0, {5.0, 7.0}) == LLUtils::PointI32{25, 33});
    REQUIRE(OIV::SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(OIV::SelectionRect::Operation::Drag));
    REQUIRE_FALSE(OIV::SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(OIV::SelectionRect::Operation::NoOp));
}

TEST_CASE("ViewTransformController calculates fit, center, and coordinate transforms", "[AppCore]")
{
    REQUIRE(OIV::ViewTransformController::FitScale({800.0, 600.0}, {400.0, 400.0}) == Catch::Approx(1.5));

    const LLUtils::PointF64 offset = OIV::ViewTransformController::CenterOffset({400.0, 300.0}, {200.0, 100.0});
    REQUIRE(offset == LLUtils::PointF64{300.0, 250.0});

    const LLUtils::PointF64 clientPoint = OIV::ViewTransformController::ImageToClient(LLUtils::PointF64{10.0, 20.0},
                                                                                      2.0, {5.0, 7.0});
    REQUIRE(clientPoint == LLUtils::PointF64{25.0, 47.0});
    REQUIRE(OIV::ViewTransformController::ClientToImage(clientPoint, 2.0, {5.0, 7.0}) == LLUtils::PointF64{10.0, 20.0});
}

TEST_CASE("ViewTransformController bounds offsets using configured margins", "[AppCore]")
{
    REQUIRE(OIV::ViewTransformController::ResolveOffset({500.0, -500.0}, {100.0, 100.0}, {300.0, 300.0},
                                                        {0.75, 0.75}) == LLUtils::PointF64{75.0, -275.0});

    REQUIRE(OIV::ViewTransformController::ResolveOffset({-100.0, 100.0}, {300.0, 300.0}, {100.0, 100.0},
                                                        {0.75, 0.75}) == LLUtils::PointF64{-75.0, 100.0});
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
            LLUTILS_TEXT("<textcolor=#ff8930>Animation speed<textcolor=#7672ff> (125.00%)"));
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
    REQUIRE(OIV::FileRemovalPolicy::Decide(LLUTILS_TEXT("a.png"), LLUTILS_TEXT("b.png"), {}, true, true, 2) ==
            OIV::RemovedFileAction::Ignore);
    REQUIRE(OIV::FileRemovalPolicy::Decide(LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), false,
                                           true, 2) == OIV::RemovedFileAction::KeepMissingCurrent);
    REQUIRE(OIV::FileRemovalPolicy::Decide(LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), {}, true, false, 2) ==
            OIV::RemovedFileAction::KeepMissingCurrent);
    REQUIRE(OIV::FileRemovalPolicy::Decide(LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), true,
                                           true, 1) == OIV::RemovedFileAction::TryStart);
    REQUIRE(OIV::FileRemovalPolicy::Decide(LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), LLUTILS_TEXT("a.png"), true,
                                           true, 3) == OIV::RemovedFileAction::TryNextThenPrevious);
}

TEST_CASE("FileRemovalPolicy unloads only after failed removal jumps", "[AppCore]")
{
    REQUIRE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryStart, false, false));
    REQUIRE_FALSE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryStart, true, false));
    REQUIRE(OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryNextThenPrevious, false, false));
    REQUIRE_FALSE(
        OIV::FileRemovalPolicy::ShouldUnloadAfterJumps(OIV::RemovedFileAction::TryNextThenPrevious, false, true));
}

TEST_CASE("FileChangePolicy routes watcher events", "[AppCore]")
{
    using Op = OIV::IFileWatcher::FileChangedOp;

    const auto basePath   = std::filesystem::path{} / "images";
    const auto configPath = std::filesystem::path{} / "config";
    const auto otherPath  = std::filesystem::path{} / "other";

    OIV::IFileWatcher::FileChangedEventArgs eventArgs{7, Op::Modified, basePath.native(), LLUTILS_TEXT("a.png"), {}};

    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, (basePath / "a.png").native()) ==
            OIV::FileChangeAction::CurrentFileChanged);
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, (basePath / "b.png").native()) ==
            OIV::FileChangeAction::Ignore);

    eventArgs = {7, Op::Rename, basePath.native(), LLUTILS_TEXT("old.png"), LLUTILS_TEXT("new.png")};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, (basePath / "new.png").native()) ==
            OIV::FileChangeAction::CurrentFileChanged);

    eventArgs = {7, Op::WatchedFolderRemoved, basePath.native(), {}, {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) == OIV::FileChangeAction::ClearWatchedFolder);

    eventArgs = {9, Op::Modified, configPath.native(), LLUTILS_TEXT("Settings.json"), {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) == OIV::FileChangeAction::ReloadSettings);

    eventArgs = {11, Op::Modified, otherPath.native(), LLUTILS_TEXT("x.png"), {}};
    REQUIRE(OIV::FileChangePolicy::Decide(eventArgs, true, 7, 9, {}) == OIV::FileChangeAction::Ignore);
}

TEST_CASE("FileReloadPolicy decides reload timing", "[AppCore]")
{
    OIV::FileReloadPolicy policy;

    policy.SetMode(OIV::FileReloadMode::None);
    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("a.png"), true) == OIV::ReloadAction::None);

    policy.SetMode(OIV::FileReloadMode::AutoBackground);
    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("a.png"), false) == OIV::ReloadAction::RequestNow);

    policy.SetMode(OIV::FileReloadMode::AutoForeground);
    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("a.png"), true) == OIV::ReloadAction::RequestNow);
    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("a.png"), false) == OIV::ReloadAction::Defer);
    REQUIRE(policy.HasPendingReloadFor(LLUTILS_TEXT("a.png")));
    REQUIRE(policy.OnPendingReloadRequested(LLUTILS_TEXT("a.png")) == OIV::ReloadAction::RequestNow);
    REQUIRE_FALSE(policy.HasPendingReloadFor(LLUTILS_TEXT("a.png")));
}

TEST_CASE("FileReloadPolicy owns confirmation pending state", "[AppCore]")
{
    OIV::FileReloadPolicy policy;
    policy.SetMode(OIV::FileReloadMode::Confirmation);

    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("a.png"), false) == OIV::ReloadAction::Defer);
    REQUIRE(policy.GetPendingReloadFile() == LLUTILS_TEXT("a.png"));
    REQUIRE(policy.OnPendingReloadRequested(LLUTILS_TEXT("a.png")) == OIV::ReloadAction::AskUser);
    REQUIRE(policy.ConfirmReload(false) == OIV::ReloadAction::None);
    REQUIRE(policy.GetPendingReloadFile().empty());

    REQUIRE(policy.OnCurrentFileChanged(LLUTILS_TEXT("b.png"), true) == OIV::ReloadAction::AskUser);
    REQUIRE(policy.ConfirmReload(true) == OIV::ReloadAction::RequestNow);
    REQUIRE(policy.GetPendingReloadFile().empty());
}

TEST_CASE("AppSettingsPolicy parses typed settings values", "[AppCore]")
{
    REQUIRE(OIV::AppSettingsPolicy::ParseIntegral(LLUTILS_TEXT("42")) == 42);
    REQUIRE(OIV::AppSettingsPolicy::ParseFloat(LLUTILS_TEXT("1.5")) == Catch::Approx(1.5));
    REQUIRE(OIV::AppSettingsPolicy::ParseBool(LLUTILS_TEXT("true")));
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseBool(LLUTILS_TEXT("false")));

    auto deletedMode = OIV::AppSettingsPolicy::ParseDeletedFileRemovalMode(LLUTILS_TEXT("always"));
    REQUIRE(deletedMode.valid);
    REQUIRE((deletedMode.value & OIV::DeletedFileRemovalMode::DeletedInternally) ==
            OIV::DeletedFileRemovalMode::DeletedInternally);
    REQUIRE((deletedMode.value & OIV::DeletedFileRemovalMode::DeletedExternally) ==
            OIV::DeletedFileRemovalMode::DeletedExternally);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseDeletedFileRemovalMode(LLUTILS_TEXT("bad")).valid);

    auto reloadMode = OIV::AppSettingsPolicy::ParseFileReloadMode(LLUTILS_TEXT("autoforeground"));
    REQUIRE(reloadMode.valid);
    REQUIRE(reloadMode.value == OIV::FileReloadMode::AutoForeground);

    auto sortType = OIV::AppSettingsPolicy::ParseSortType(LLUTILS_TEXT("extension"));
    REQUIRE(sortType.valid);
    REQUIRE(sortType.value == OIV::FileSorter::SortType::Extension);
    REQUIRE(OIV::AppSettingsPolicy::ParseSortDirection(LLUTILS_TEXT("ascending")) ==
            OIV::FileSorter::SortDirection::Ascending);
    REQUIRE(OIV::AppSettingsPolicy::ParseSortDirection(LLUTILS_TEXT("descending")) ==
            OIV::FileSorter::SortDirection::Descending);

    auto sortDirectionTarget = OIV::AppSettingsPolicy::ParseSortDirectionTarget(
        LLUTILS_TEXT("files/sortbydatedirection"));
    REQUIRE(sortDirectionTarget.valid);
    REQUIRE(sortDirectionTarget.value == OIV::FileSorter::SortType::Date);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseSortDirectionTarget(LLUTILS_TEXT("files/unknown")).valid);

    auto backgroundColorIndex = OIV::AppSettingsPolicy::ParseBackgroundColorIndex(
        LLUTILS_TEXT("displaysettings/backgroundcolor2"));
    REQUIRE(backgroundColorIndex.valid);
    REQUIRE(backgroundColorIndex.value == 1);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::ParseBackgroundColorIndex(LLUTILS_TEXT("displaysettings/other")).valid);
}

TEST_CASE("AppSettingsPolicy maps setting changes to typed actions", "[AppCore]")
{
    using ActionType = OIV::AppSettingsPolicy::ActionType;

    auto action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/maxzoom"), LLUTILS_TEXT("12.5"));
    REQUIRE(action.type == ActionType::MaxZoom);
    REQUIRE(action.floatValue == Catch::Approx(12.5));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/imagemargins/x"), LLUTILS_TEXT("0.25"));
    REQUIRE(action.type == ActionType::ImageMarginX);
    REQUIRE(action.floatValue == Catch::Approx(0.25));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/imagemargins/y"), LLUTILS_TEXT("0.5"));
    REQUIRE(action.type == ActionType::ImageMarginY);
    REQUIRE(action.floatValue == Catch::Approx(0.5));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/minimagesize"), LLUTILS_TEXT("128"));
    REQUIRE(action.type == ActionType::MinImageSize);
    REQUIRE(action.floatValue == Catch::Approx(128.0));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/slideshowinterval"), LLUTILS_TEXT("1500"));
    REQUIRE(action.type == ActionType::SlideshowInterval);
    REQUIRE(action.integralValue == 1500);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/quickbrowsedelay"), LLUTILS_TEXT("75"));
    REQUIRE(action.type == ActionType::QuickBrowseDelay);
    REQUIRE(action.integralValue == 75);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("autoscroll/deadzoneradius"), LLUTILS_TEXT("40"));
    REQUIRE(action.type == ActionType::AutoScrollDeadZoneRadius);
    REQUIRE(action.integralValue == 40);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("autoscroll/speedfactorin"), LLUTILS_TEXT("1.25"));
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorIn);
    REQUIRE(action.floatValue == Catch::Approx(1.25));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("autoscroll/speedfactorout"), LLUTILS_TEXT("2.5"));
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorOut);
    REQUIRE(action.floatValue == Catch::Approx(2.5));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("autoscroll/speedfactorrange"), LLUTILS_TEXT("6"));
    REQUIRE(action.type == ActionType::AutoScrollSpeedFactorRange);
    REQUIRE(action.integralValue == 6);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("autoscroll/maxspeed"), LLUTILS_TEXT("120"));
    REQUIRE(action.type == ActionType::AutoScrollMaxSpeed);
    REQUIRE(action.integralValue == 120);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("filesystem/deletedfileremovalmode"),
                                                 LLUTILS_TEXT("externally"));
    REQUIRE(action.type == ActionType::DeletedFileRemovalMode);
    REQUIRE(action.deletedFileRemovalMode == OIV::DeletedFileRemovalMode::DeletedExternally);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("filesystem/modifiedfilereloadmode"),
                                                 LLUTILS_TEXT("autobackground"));
    REQUIRE(action.type == ActionType::FileReloadMode);
    REQUIRE(action.fileReloadMode == OIV::FileReloadMode::AutoBackground);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("system/reloadsettingsfileifchanged"),
                                                 LLUTILS_TEXT("true"));
    REQUIRE(action.type == ActionType::ReloadSettingsFileIfChanged);
    REQUIRE(action.boolValue);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("files/defaultsortmode"), LLUTILS_TEXT("extension"));
    REQUIRE(action.type == ActionType::DefaultSortMode);
    REQUIRE(action.sortType == OIV::FileSorter::SortType::Extension);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("files/sortbydatedirection"), LLUTILS_TEXT("descending"));
    REQUIRE(action.type == ActionType::SortDirection);
    REQUIRE(action.sortType == OIV::FileSorter::SortType::Date);
    REQUIRE(action.sortDirection == OIV::FileSorter::SortDirection::Descending);

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("displaysettings/backgroundcolor2"),
                                                 LLUTILS_TEXT("#11223344"));
    REQUIRE(action.type == ActionType::BackgroundColor);
    REQUIRE(action.backgroundColorIndex == 1);
    REQUIRE(action.textValue == LLUTILS_TEXT("#11223344"));

    action = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("imagesettings/biggestsubimage"), LLUTILS_TEXT("false"));
    REQUIRE(action.type == ActionType::BiggestSubImageOnLoad);
    REQUIRE_FALSE(action.boolValue);

    REQUIRE(OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("unknown/key"), LLUTILS_TEXT("value")).type ==
            ActionType::None);
    REQUIRE(OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("filesystem/deletedfileremovalmode"), LLUTILS_TEXT("bad"))
                .type == ActionType::None);
    REQUIRE(OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("files/defaultsortmode"), LLUTILS_TEXT("bad")).type ==
            ActionType::None);
}

TEST_CASE("ImageFormatCatalogPolicy builds image dialog filters", "[AppCore]")
{
    const auto catalog = OIV::ImageFormatCatalogPolicy::Build(
        {CreatePlugin(IMCodec::CodecCapabilities::Decode | IMCodec::CodecCapabilities::Encode,
                      LLUTILS_TEXT("PNG image"), {LLUTILS_TEXT("PNG"), LLUTILS_TEXT("Apng")}),
         CreatePlugin(IMCodec::CodecCapabilities::Decode, LLUTILS_TEXT("JPEG image"),
                      {LLUTILS_TEXT("jpg"), LLUTILS_TEXT("jpeg")}),
         CreatePlugin(IMCodec::CodecCapabilities::Encode | IMCodec::CodecCapabilities::BulkCodec,
                      LLUTILS_TEXT("Bulk writer"), {LLUTILS_TEXT("bulk")})});

    REQUIRE(catalog.readFilters.size() == 4);
    REQUIRE(catalog.readFilters[0].description == LLUTILS_TEXT("All files (*.*)"));
    REQUIRE(catalog.readFilters[0].extensions == std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("*.*")});
    REQUIRE(catalog.readFilters[1].description == LLUTILS_TEXT("All supported image formats"));
    REQUIRE(catalog.readFilters[1].extensions ==
            std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("*.png"), LLUTILS_TEXT("*.apng"),
                                                     LLUTILS_TEXT("*.jpg"), LLUTILS_TEXT("*.jpeg")});
    REQUIRE(catalog.readFilters[2].description == LLUTILS_TEXT("PNG/APNG - PNG image"));
    REQUIRE(catalog.readFilters[2].extensions ==
            std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("*.png"), LLUTILS_TEXT("*.apng")});

    REQUIRE(catalog.writeFilters.size() == 1);
    REQUIRE(catalog.writeFilters[0].description == LLUTILS_TEXT("PNG/APNG - PNG image"));
    REQUIRE(catalog.writeFilters[0].extensions == std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("*.png")});
    REQUIRE(catalog.defaultSaveFileFormatIndex == 1);
    REQUIRE(catalog.defaultSaveFileExtension == LLUTILS_TEXT("png"));
    REQUIRE(catalog.knownFileTypesSet ==
            std::set<LLUtils::native_string_type>{LLUTILS_TEXT("apng"), LLUTILS_TEXT("jpeg"), LLUTILS_TEXT("jpg"),
                                                  LLUTILS_TEXT("png")});
    REQUIRE(catalog.knownFileTypes == LLUTILS_TEXT("apng;jpeg;jpg;png"));
}

TEST_CASE("ImageFormatCatalogPolicy defaults save filter index without PNG", "[AppCore]")
{
    const auto catalog = OIV::ImageFormatCatalogPolicy::Build(
        {CreatePlugin(IMCodec::CodecCapabilities::Encode, LLUTILS_TEXT("JPEG image"), {LLUTILS_TEXT("jpg")})});

    REQUIRE(catalog.writeFilters.size() == 1);
    REQUIRE(catalog.writeFilters[0].extensions == std::vector<LLUtils::native_string_type>{LLUTILS_TEXT("*.jpg")});
    REQUIRE(catalog.defaultSaveFileFormatIndex == 0);
}

TEST_CASE("ImageInfoPresentationPolicy builds file image rows", "[AppCore]")
{
    const auto folder   = MakeTempFolder("oiv-image-info-policy-test");
    const auto filePath = folder / LLUTILS_TEXT("image.bin");
    TouchFile(filePath);

    IMCodec::ImageSharedPtr image                  = CreateFormattedTestImage(16, 8);
    image->GetImageItem()->processData.processTime = 12.5;

    auto fileImage = std::make_shared<OIV::OIVFileImage>(filePath.native(), image);
    fileImage->SetDisplayTime(3.25);
    fileImage->SetNumUniqueColors(7);

    auto metaData               = std::make_shared<IMCodec::ItemMetaData>();
    metaData->exifData.latitude = 1.25;
    metaData->exifData.make     = "CameraBrand";
    fileImage->SetMetaData(metaData);

    FakeImageCodec codec;
    const auto rows = OIV::ImageInfoPresentationPolicy::Build(fileImage, fileImage, codec);

    const auto* filePathRow = FindRow(rows, "File path");
    REQUIRE(filePathRow != nullptr);
    REQUIRE(filePathRow->values.at(0).text == filePath.native());

    const auto* fileSizeRow = FindRow(rows, "File size");
    REQUIRE(fileSizeRow != nullptr);
    REQUIRE(fileSizeRow->values.at(0).text == LLUTILS_TEXT("4 bytes"));

    const auto* widthRow = FindRow(rows, "Width");
    REQUIRE(widthRow != nullptr);
    REQUIRE(widthRow->values.at(0).text == LLUTILS_TEXT("16"));
    REQUIRE(widthRow->values.at(1).text == LLUTILS_TEXT("px"));

    const auto* channelsRow = FindRow(rows, "channels info");
    REQUIRE(channelsRow != nullptr);
    REQUIRE(channelsRow->values.at(0).text == LLUTILS_TEXT("R:8 G:8 B:8 A:8"));

    const auto* codecRow = FindRow(rows, "Codec used");
    REQUIRE(codecRow != nullptr);
    REQUIRE(codecRow->values.at(0).text == LLUTILS_TEXT("Fake codec"));

    const auto* latitudeRow = FindRow(rows, "Latitude");
    REQUIRE(latitudeRow != nullptr);
    REQUIRE(latitudeRow->values.at(0).text == LLUTILS_TEXT("1.250000"));

    const auto* manufacturerRow = FindRow(rows, "Manufacturer");
    REQUIRE(manufacturerRow != nullptr);
    REQUIRE(manufacturerRow->values.at(0).text == LLUTILS_TEXT("CameraBrand"));
}

TEST_CASE("ImageInfoPresentationPolicy handles generated animation images", "[AppCore]")
{
    auto containerItem                                = std::make_shared<IMCodec::ImageItem>();
    containerItem->itemType                           = IMCodec::ImageItemType::Container;
    containerItem->descriptor.width                   = 4;
    containerItem->descriptor.height                  = 4;
    containerItem->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
    containerItem->descriptor.texelFormatStorage      = IMCodec::TexelFormat::I_R8_G8_B8_A8;
    containerItem->descriptor.rowPitchInBytes         = 16;

    IMCodec::ImageSharedPtr container = std::make_shared<IMCodec::Image>(containerItem,
                                                                         IMCodec::ImageItemType::AnimationFrame);
    container->SetSubImage(0, CreateFormattedTestImage(4, 2));

    auto image = std::make_shared<OIV::OIVBaseImage>(OIV::ImageSource::GeneratedByLib, container);
    FakeImageCodec codec;
    const auto rows = OIV::ImageInfoPresentationPolicy::Build(image, image, codec);

    const auto* sourceRow = FindRow(rows, "Source");
    REQUIRE(sourceRow != nullptr);
    REQUIRE(sourceRow->values.at(0).text == LLUTILS_TEXT("auto generated"));

    const auto* frameRow = FindRow(rows, "Num frames");
    REQUIRE(frameRow != nullptr);
    REQUIRE(frameRow->values.at(0).text == LLUTILS_TEXT("1"));

    const auto* heightRow = FindRow(rows, "Height");
    REQUIRE(heightRow != nullptr);
    REQUIRE(heightRow->values.at(0).text == LLUTILS_TEXT("2"));
}

TEST_CASE("FileSorter orders files by name and extension", "[AppCore][Shared]")
{
    OIV::FileSorter sorter;
    const auto basePath = std::filesystem::path{} / "images";

    const LLUtils::native_string_type png = (basePath / "b.png").native();
    const LLUtils::native_string_type jpg = (basePath / "a.jpg").native();

    sorter.SetSortType(OIV::FileSorter::SortType::Name);
    REQUIRE(sorter(jpg, png));

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    REQUIRE(sorter(jpg, png));
}

TEST_CASE("Event connection disconnects lambda listeners", "[AppCore][LLUtils]")
{
    LLUtils::Event<void(int)> event;
    int firstCount  = 0;
    int secondCount = 0;

    auto firstConnection  = event.Connect([&](int value) { firstCount += value; });
    auto secondConnection = event.Connect([&](int value) { secondCount += value; });

    event.Raise(2);
    REQUIRE(firstCount == 2);
    REQUIRE(secondCount == 2);

    firstConnection.Disconnect();
    event.Raise(3);

    REQUIRE(firstCount == 2);
    REQUIRE(secondCount == 5);
}

TEST_CASE("Event connection ownership can move and disconnect explicitly", "[AppCore][LLUtils]")
{
    LLUtils::Event<void()> event;
    int count = 0;

    auto connection      = event.Connect([&] { ++count; });
    auto movedConnection = std::move(connection);

    event.Raise();
    REQUIRE(count == 1);

    movedConnection.Disconnect();
    movedConnection.Disconnect();
    event.Raise();

    REQUIRE(count == 1);
}

TEST_CASE("Event connection disconnects automatically on destruction", "[AppCore][LLUtils]")
{
    LLUtils::Event<void()> event;
    int count = 0;

    {
        auto connection = event.Connect([&] { ++count; });
        event.Raise();
        REQUIRE(count == 1);
    }

    event.Raise();
    REQUIRE(count == 1);
}

TEST_CASE("Event connection can disconnect while event is being raised", "[AppCore][LLUtils]")
{
    LLUtils::Event<void()> event;
    LLUtils::Event<void()>::Connection skippedConnection;
    LLUtils::Event<void()>::Connection selfConnection;
    std::vector<int> order;
    int selfCount = 0;

    auto firstConnection = event.Connect(
        [&]
        {
            order.push_back(1);
            skippedConnection.Disconnect();
        });
    skippedConnection = event.Connect([&] { order.push_back(2); });
    selfConnection    = event.Connect(
        [&]
        {
            ++selfCount;
            selfConnection.Disconnect();
        });

    event.Raise();

    REQUIRE(order == std::vector<int>{1});
    REQUIRE(selfCount == 1);

    order.clear();
    event.Raise();

    REQUIRE(order == std::vector<int>{1});
    REQUIRE(selfCount == 1);
}

TEST_CASE("Event connection handles nested raises without compacting early", "[AppCore][LLUtils]")
{
    LLUtils::Event<void()> event;
    LLUtils::Event<void()>::Connection skippedConnection;
    int firstCount  = 0;
    int secondCount = 0;

    auto firstConnection = event.Connect(
        [&]
        {
            ++firstCount;

            if (firstCount == 1)
            {
                skippedConnection.Disconnect();
                event.Raise();
            }
        });
    skippedConnection = event.Connect([&] { ++secondCount; });

    event.Raise();

    firstConnection.Disconnect();
    event.Raise();

    REQUIRE(firstCount == 2);
    REQUIRE(secondCount == 0);
}

TEST_CASE("Event connection restores raise state when listener throws", "[AppCore][LLUtils]")
{
    LLUtils::Event<void()> event;
    int count = 0;

    auto throwingConnection = event.Connect([] { throw std::runtime_error("event failure"); });
    auto countConnection    = event.Connect([&] { ++count; });

    REQUIRE_THROWS_AS(event.Raise(), std::runtime_error);

    throwingConnection.Disconnect();
    event.Raise();

    REQUIRE(count == 1);

    countConnection.Disconnect();
}

TEST_CASE("Event Add keeps fire-and-forget listener compatibility", "[AppCore][LLUtils]")
{
    LLUtils::Event<void(int)> event;
    int count = 0;

    event.Add([&](int value) { count += value; });
    event.Raise(4);

    REQUIRE(count == 4);
}

TEST_CASE("FolderFileList updates the current folder list from explicit file changes", "[AppCore]")
{
    const auto folder = std::filesystem::temp_directory_path() / "oiv-file-list-test";
    const auto fileA  = folder / "a.png";
    const auto fileB  = folder / "b.png";
    const auto fileC  = folder / "c.png";
    std::filesystem::create_directories(folder);

    OIV::FileSorter sorter;
    std::vector<std::pair<OIV::FolderFileList::index_type, OIV::FolderFileList::index_type>> indexChanges;

    OIV::FolderFileList fileList(&sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"),
                                 [&](auto current, auto previous) { indexChanges.push_back({current, previous}); });

    fileList.SetFolder(folder.native(), {fileA.native(), fileC.native()}, fileA.native());
    REQUIRE(fileList.GetSize() == 2);
    REQUIRE(fileList.GetCurrentItemName() == fileA.native());

    fileList.UpdateFolderFileList(OIV::IFileWatcher::FileChangedOp::Add, fileB.native(), {}, fileA.native());
    REQUIRE(fileList.GetSize() == 3);
    REQUIRE(fileList.GetElementNameFromIndex(1) == fileB.native());
    REQUIRE(fileList.GetCurrentItemName() == fileA.native());

    fileList.UpdateFolderFileList(OIV::IFileWatcher::FileChangedOp::Remove, fileB.native(), {}, fileA.native());
    REQUIRE(fileList.GetSize() == 2);
    REQUIRE(fileList.GetElementNameFromIndex(1) == fileC.native());

    const auto fileBText = folder / "b.txt";
    fileList.UpdateFolderFileList(OIV::IFileWatcher::FileChangedOp::Rename, fileC.native(), fileBText.native(),
                                  fileA.native());
    REQUIRE(fileList.GetSize() == 1);
    REQUIRE(fileList.GetCurrentItemName() == fileA.native());

    fileList.UpdateFolderFileList(OIV::IFileWatcher::FileChangedOp::Rename, fileBText.native(), fileC.native(),
                                  fileA.native());
    REQUIRE(fileList.GetSize() == 2);
    REQUIRE(fileList.GetElementNameFromIndex(1) == fileC.native());
}

TEST_CASE("FolderFileList sorts files loaded from disk for every sort mode", "[AppCore]")
{
    const auto folder  = MakeTempFolder("oiv-file-list-disk-sort-test");
    const auto filePng = folder / LLUTILS_TEXT("a.png");
    const auto fileBmp = folder / LLUTILS_TEXT("b.bmp");
    const auto fileJpg = folder / LLUTILS_TEXT("c.jpg");
    TouchFile(filePng);
    TouchFile(fileBmp);
    TouchFile(fileJpg);

    const auto baseTime = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(filePng, baseTime - std::chrono::hours(2));
    std::filesystem::last_write_time(fileBmp, baseTime);
    std::filesystem::last_write_time(fileJpg, baseTime - std::chrono::hours(1));

    auto requireLoadedOrder = [&](OIV::FileSorter::SortType sortType, OIV::FileSorter::SortDirection sortDirection,
                                  const std::vector<std::filesystem::path>& expectedFiles,
                                  const std::filesystem::path& currentFile)
    {
        OIV::FileSorter sorter;
        sorter.SetSortType(sortType);
        sorter.SetActiveSortDirection(sortDirection);

        OIV::FolderFileList fileList(&sorter, {LLUTILS_TEXT("bmp"), LLUTILS_TEXT("jpg"), LLUTILS_TEXT("png")},
                                     LLUTILS_TEXT("bmp;jpg;png"), [](auto, auto) {});

        fileList.SetFolder(folder.native(), {}, currentFile.native());

        REQUIRE(fileList.GetSize() == expectedFiles.size());
        for (std::size_t index = 0; index < expectedFiles.size(); ++index)
            REQUIRE(fileList.GetElementNameFromIndex(static_cast<OIV::FolderFileList::index_type>(index)) ==
                    expectedFiles[index].native());
        REQUIRE(fileList.GetCurrentIndex() == 1);
        REQUIRE(fileList.GetCurrentItemName() == currentFile.native());
    };

    requireLoadedOrder(OIV::FileSorter::SortType::Name, OIV::FileSorter::SortDirection::Descending,
                       {fileJpg, fileBmp, filePng}, fileBmp);
    requireLoadedOrder(OIV::FileSorter::SortType::Extension, OIV::FileSorter::SortDirection::Ascending,
                       {fileBmp, fileJpg, filePng}, fileJpg);
    requireLoadedOrder(OIV::FileSorter::SortType::Date, OIV::FileSorter::SortDirection::Descending,
                       {fileBmp, fileJpg, filePng}, fileJpg);
}

TEST_CASE("FolderFileList leaves current index invalid when current file is not represented", "[AppCore]")
{
    const auto folder = std::filesystem::temp_directory_path() / "oiv-file-list-unrepresented-test";
    const auto fileA  = folder / "a.png";
    const auto fileB  = folder / "b.txt";
    std::filesystem::create_directories(folder);

    OIV::FileSorter sorter;
    std::vector<std::pair<OIV::FolderFileList::index_type, OIV::FolderFileList::index_type>> indexChanges;

    OIV::FolderFileList fileList(&sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"),
                                 [&](auto current, auto previous) { indexChanges.push_back({current, previous}); });

    fileList.SetFolder(folder.native(), {fileA.native()}, fileB.native());

    REQUIRE(fileList.GetSize() == 1);
    REQUIRE_FALSE(fileList.IsIndexValid(fileList.GetCurrentIndex()));
    REQUIRE(indexChanges.empty());
}

TEST_CASE("ImageOpenController classifies load results", "[AppCore]")
{
    REQUIRE(OIV::ImageOpenController::ClassifyLoadResult(ResultCode::RC_Success, CreateTestImage()) ==
            OIV::ImageLoadStatus::Loaded);
    REQUIRE(OIV::ImageOpenController::ClassifyLoadResult(ResultCode::RC_FileNotSupported, nullptr) ==
            OIV::ImageLoadStatus::UnsupportedFormat);
    REQUIRE(OIV::ImageOpenController::ClassifyLoadResult(ResultCode::RC_UknownError, nullptr) ==
            OIV::ImageLoadStatus::UnknownError);
    REQUIRE(OIV::ImageOpenController::ClassifyLoadResult(
                ResultCode::RC_Success, CreateTestImage(OIV::ImageOpenController::MaxSupportedDimension + 1, 1)) ==
            OIV::ImageLoadStatus::TooLarge);
}

TEST_CASE("ImageLoadPresentationPolicy maps load results to UI decisions", "[AppCore]")
{
    OIV::ImageLoadResult result{OIV::ImageLoadStatus::FolderLoadQueued};
    auto presentation = OIV::ImageLoadPresentationPolicy::Decide(result, LLUTILS_TEXT("path.png"));
    REQUIRE(presentation.succeeded);
    REQUIRE_FALSE(presentation.shouldLoadImage);
    REQUIRE_FALSE(presentation.shouldShowMessage);

    result       = {OIV::ImageLoadStatus::Loaded, ResultCode::RC_Success, LLUTILS_TEXT("path.png"), nullptr};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, LLUTILS_TEXT("path.png"));
    REQUIRE(presentation.succeeded);
    REQUIRE(presentation.shouldLoadImage);

    result       = {OIV::ImageLoadStatus::UnsupportedFormat, ResultCode::RC_FileNotSupported, LLUTILS_TEXT("path.png"),
                    nullptr};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, LLUTILS_TEXT("<path>"));
    REQUIRE_FALSE(presentation.succeeded);
    REQUIRE(presentation.shouldShowMessage);
    REQUIRE(presentation.message == LLUTILS_TEXT("Can not load the file: <path>, image format is not supported"));

    result       = {OIV::ImageLoadStatus::NoSupportedFiles};
    presentation = OIV::ImageLoadPresentationPolicy::Decide(result, LLUTILS_TEXT("path.png"));
    REQUIRE_FALSE(presentation.succeeded);
    REQUIRE_FALSE(presentation.shouldShowMessage);
}

TEST_CASE("ImageOpenController loads direct files through the injected loader", "[AppCore]")
{
    auto fakeLoader     = std::make_unique<FakeImageFileLoader>();
    auto* fakeLoaderPtr = fakeLoader.get();
    OIV::ImageOpenController controller(std::move(fakeLoader));

    const auto sourcePath = std::filesystem::path{} / "images" / ".." / "images" / "a.png";

    const auto result = controller.LoadFile(sourcePath.native(), IMCodec::PluginTraverseMode::AnyPlugin,
                                            OIV::ImageLoadContext{800, 600});

    REQUIRE(result.status == OIV::ImageLoadStatus::Loaded);
    REQUIRE(result.DecodeSucceeded());
    REQUIRE(result.image != nullptr);
    REQUIRE(fakeLoaderPtr->lastPath == sourcePath.lexically_normal().native());
    REQUIRE(fakeLoaderPtr->lastTraverseMode == IMCodec::PluginTraverseMode::AnyPlugin);
    REQUIRE(fakeLoaderPtr->lastContext.canvasWidth == 800);
    REQUIRE(fakeLoaderPtr->lastContext.canvasHeight == 600);
}

TEST_CASE("BrowseSessionController sets current index for loaded file in folder", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-load-index-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
                                            [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {});

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    REQUIRE(controller.CommitCurrentFile(fileC.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 2);
    REQUIRE(controller.IsCurrentFile(fileC.native()));
}

TEST_CASE("BrowseSessionController updates current index after sort order changes", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-sort-index-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.jpg");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("jpg"), LLUTILS_TEXT("png")},
                                            LLUTILS_TEXT("jpg;png"), residency,
                                            [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {});

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    controller.SortFolderFileList();
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 0);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    sorter.SetSortType(OIV::FileSorter::SortType::Name);
    controller.SortFolderFileList();
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);
    REQUIRE(controller.IsCurrentFile(fileB.native()));
}

TEST_CASE("BrowseSessionController ignores stale navigation success after sort order changes", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-sort-pending-success-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.jpg");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    std::mutex completionMutex;
    std::condition_variable completionCv;
    std::vector<OIV::BrowseSessionController::BrowseCandidateCompletion> completions;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("jpg"), LLUTILS_TEXT("png")}, LLUTILS_TEXT("jpg;png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            {
                std::lock_guard lock(completionMutex);
                completions.push_back(completion);
            }
            completionCv.notify_all();
        });

    auto waitForCompletionCount = [&](std::size_t count)
    {
        std::unique_lock lock(completionMutex);
        REQUIRE(
            completionCv.wait_for(lock, std::chrono::milliseconds(500), [&] { return completions.size() >= count; }));
        return completions[count - 1];
    };

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);
    REQUIRE(controller.JumpFiles(1));

    const auto staleCompletion = waitForCompletionCount(1);
    REQUIRE(staleCompletion.fileName == fileC.native());

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    controller.SortFolderFileList();
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 0);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    const auto staleResult = controller.OnBrowseCandidateReady(staleCompletion);
    REQUIRE(staleResult.action == OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    REQUIRE(controller.JumpFiles(1));
    const auto newCompletion = waitForCompletionCount(2);
    REQUIRE(newCompletion.fileName == fileA.native());

    const auto newResult = controller.OnBrowseCandidateReady(newCompletion);
    REQUIRE(newResult.action == OIV::BrowseSessionController::BrowseSessionAction::DisplayImage);
    REQUIRE(controller.IsCurrentFile(fileA.native()));
}

TEST_CASE("BrowseSessionController ignores stale navigation failure after sort order changes", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-sort-pending-failure-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.jpg");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(
        std::make_unique<SelectiveResidencyProcessor>(std::set<LLUtils::native_string_type>{}), 1);
    std::mutex completionMutex;
    std::condition_variable completionCv;
    std::vector<OIV::BrowseSessionController::BrowseCandidateCompletion> completions;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("jpg"), LLUTILS_TEXT("png")}, LLUTILS_TEXT("jpg;png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            {
                std::lock_guard lock(completionMutex);
                completions.push_back(completion);
            }
            completionCv.notify_all();
        });

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);
    REQUIRE(controller.JumpFiles(1));

    OIV::BrowseSessionController::BrowseCandidateCompletion staleCompletion;
    {
        std::unique_lock lock(completionMutex);
        REQUIRE(completionCv.wait_for(lock, std::chrono::milliseconds(500), [&] { return completions.size() == 1; }));
        staleCompletion = completions.back();
    }
    REQUIRE(staleCompletion.fileName == fileC.native());
    REQUIRE(staleCompletion.image == nullptr);

    sorter.SetSortType(OIV::FileSorter::SortType::Extension);
    controller.SortFolderFileList();

    const auto staleResult = controller.OnBrowseCandidateReady(staleCompletion);
    REQUIRE(staleResult.action == OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    std::lock_guard lock(completionMutex);
    REQUIRE(completions.size() == 1);
}

TEST_CASE("BrowseSessionController accepts normalized pending file identity", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-normalized-pending-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    TouchFile(fileA);
    TouchFile(fileB);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    std::mutex completionMutex;
    std::condition_variable completionCv;
    std::vector<OIV::BrowseSessionController::BrowseCandidateCompletion> completions;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            {
                std::lock_guard lock(completionMutex);
                completions.push_back(completion);
            }
            completionCv.notify_all();
        });

    REQUIRE(controller.CommitCurrentFile(fileA.native()) == ResultCode::RC_Success);
    REQUIRE(controller.JumpFiles(1));

    OIV::BrowseSessionController::BrowseCandidateCompletion completion;
    {
        std::unique_lock lock(completionMutex);
        REQUIRE(
            completionCv.wait_for(lock, std::chrono::milliseconds(500), [&] { return completions.empty() == false; }));
        completion = completions.back();
    }

    completion.fileName = (folder / LLUTILS_TEXT(".") / LLUTILS_TEXT("b.png")).native();
    const auto result   = controller.OnBrowseCandidateReady(completion);

    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::DisplayImage);
    REQUIRE(result.fileName == fileB.native());
    REQUIRE(controller.IsCurrentFile(fileB.native()));
}

TEST_CASE("BrowseSessionController ignores stale navigation success after watched folder changes", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-watch-pending-success-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    const auto fileD  = folder / LLUTILS_TEXT("aa.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    std::mutex completionMutex;
    std::condition_variable completionCv;
    std::vector<OIV::BrowseSessionController::BrowseCandidateCompletion> completions;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            {
                std::lock_guard lock(completionMutex);
                completions.push_back(completion);
            }
            completionCv.notify_all();
        });

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);
    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 1);
    REQUIRE(controller.JumpFiles(1));

    OIV::BrowseSessionController::BrowseCandidateCompletion staleCompletion;
    {
        std::unique_lock lock(completionMutex);
        REQUIRE(
            completionCv.wait_for(lock, std::chrono::milliseconds(500), [&] { return completions.empty() == false; }));
        staleCompletion = completions.back();
    }
    REQUIRE(staleCompletion.fileName == fileC.native());

    TouchFile(fileD);
    controller.OnFileChanged({controller.GetActiveFolderID(),
                              OIV::IFileWatcher::FileChangedOp::Add,
                              folder.native(),
                              LLUTILS_TEXT("aa.png"),
                              {}});

    REQUIRE(controller.GetFolderFileList().GetCurrentIndex() == 2);
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    const auto staleResult = controller.OnBrowseCandidateReady(staleCompletion);
    REQUIRE(staleResult.action == OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.IsCurrentFile(fileB.native()));
}

TEST_CASE("BrowseSessionController returns removal result when committed current is deleted", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-current-delete-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
                                            [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {});

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);

    const auto result = controller.OnFileChanged({controller.GetActiveFolderID(),
                                                  OIV::IFileWatcher::FileChangedOp::Remove,
                                                  folder.native(),
                                                  LLUTILS_TEXT("b.png"),
                                                  {}});

    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::CurrentFileRemoved);
    REQUIRE(result.fileName == fileB.native());
    REQUIRE(controller.GetCommittedCurrentFile().empty());
    REQUIRE(controller.GetFolderFileList().GetSize() == 2);
}

TEST_CASE("BrowseSessionController returns unsupported result when committed current is renamed to unsupported type",
          "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-current-unsupported-rename-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    TouchFile(fileA);
    TouchFile(fileB);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
                                            [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {});

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);

    const auto result = controller.OnFileChanged({controller.GetActiveFolderID(),
                                                  OIV::IFileWatcher::FileChangedOp::Rename, folder.native(),
                                                  LLUTILS_TEXT("b.png"), LLUTILS_TEXT("b.txt")});

    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::CurrentFileUnsupportedRename);
    REQUIRE(result.fileName == fileB.native());
    REQUIRE(controller.GetCommittedCurrentFile().empty());
    REQUIRE(controller.GetFolderFileList().GetSize() == 1);
}

TEST_CASE("BrowseSessionController commits supported current rename and reloads renamed file", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-current-supported-rename-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileD  = folder / LLUTILS_TEXT("d.png");
    TouchFile(fileA);
    TouchFile(fileB);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    std::mutex loadedMutex;
    std::condition_variable loadedCv;
    std::vector<LLUtils::native_string_type> loadedFiles;

    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
                                            [&](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr)
                                            {
                                                {
                                                    std::lock_guard lock(loadedMutex);
                                                    loadedFiles.push_back(fileName);
                                                }
                                                loadedCv.notify_all();
                                            });

    REQUIRE(controller.CommitCurrentFile(fileB.native()) == ResultCode::RC_Success);

    const auto result = controller.OnFileChanged({controller.GetActiveFolderID(),
                                                  OIV::IFileWatcher::FileChangedOp::Rename, folder.native(),
                                                  LLUTILS_TEXT("b.png"), LLUTILS_TEXT("d.png")});

    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.IsCurrentFile(fileD.native()));
    REQUIRE(controller.GetFolderFileList().GetCurrentItemName() == fileD.native());

    std::unique_lock lock(loadedMutex);
    REQUIRE(loadedCv.wait_for(
        lock, std::chrono::milliseconds(500),
        [&] { return std::find(loadedFiles.begin(), loadedFiles.end(), fileD.native()) != loadedFiles.end(); }));
}

TEST_CASE("BrowseSessionController ignores file watcher events from inactive folders", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-inactive-watch-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    TouchFile(fileA);
    TouchFile(fileB);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController controller(&watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
                                            [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {});

    REQUIRE(controller.CommitCurrentFile(fileA.native()) == ResultCode::RC_Success);
    const auto sizeBefore = controller.GetFolderFileList().GetSize();

    const auto result = controller.OnFileChanged(
        {static_cast<OIV::IFileWatcher::FolderID>(controller.GetActiveFolderID() + 1),
         OIV::IFileWatcher::FileChangedOp::Add,
         folder.native(),
         LLUTILS_TEXT("aa.png"),
         {}});

    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.GetFolderFileList().GetSize() == sizeBefore);
    REQUIRE(controller.IsCurrentFile(fileA.native()));
}

TEST_CASE("BrowseSessionController owns folder navigation", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-navigation-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    OIV::BrowseSessionController* controllerPtr = nullptr;
    std::mutex loadedMutex;
    std::condition_variable loadedCv;
    std::vector<LLUtils::native_string_type> loadedFiles;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            const auto result = controllerPtr->OnBrowseCandidateReady(completion);
            if (result.action == OIV::BrowseSessionController::BrowseSessionAction::DisplayImage)
            {
                {
                    std::lock_guard lock(loadedMutex);
                    loadedFiles.push_back(result.fileName);
                }
                loadedCv.notify_all();
            }
        });
    controllerPtr = &controller;

    auto waitForLoaded = [&](const LLUtils::native_string_type& fileName)
    {
        std::unique_lock lock(loadedMutex);
        return loadedCv.wait_for(
            lock, std::chrono::milliseconds(500),
            [&] { return std::find(loadedFiles.begin(), loadedFiles.end(), fileName) != loadedFiles.end(); });
    };

    REQUIRE(controller.CommitCurrentFile(fileA.native()) == ResultCode::RC_Success);

    REQUIRE(controller.GetFolderFileList().GetSize() == 3);
    REQUIRE(controller.IsCurrentFile(fileA.native()));

    REQUIRE(controller.JumpFiles(1));
    REQUIRE(waitForLoaded(fileB.native()));
    REQUIRE(controller.IsCurrentFile(fileB.native()));

    REQUIRE(controller.JumpFiles(OIV::FolderFileList::IndexEnd));
    REQUIRE(waitForLoaded(fileC.native()));
    REQUIRE(controller.IsCurrentFile(fileC.native()));
}

TEST_CASE("BrowseSessionController skips invalid requested files before committing current", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-skip-invalid-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    const auto fileC  = folder / LLUTILS_TEXT("c.png");
    TouchFile(fileA);
    TouchFile(fileB);
    TouchFile(fileC);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(
        std::make_unique<SelectiveResidencyProcessor>(std::set<LLUtils::native_string_type>{fileC.native()}), 1);
    OIV::BrowseSessionController* controllerPtr = nullptr;
    std::mutex resultMutex;
    std::condition_variable resultCv;
    std::vector<LLUtils::native_string_type> loadedFiles;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            const auto result = controllerPtr->OnBrowseCandidateReady(completion);
            if (result.action == OIV::BrowseSessionController::BrowseSessionAction::DisplayImage)
            {
                {
                    std::lock_guard lock(resultMutex);
                    loadedFiles.push_back(result.fileName);
                }
                resultCv.notify_all();
            }
        });
    controllerPtr = &controller;

    REQUIRE(controller.CommitCurrentFile(fileA.native()) == ResultCode::RC_Success);
    REQUIRE(controller.IsCurrentFile(fileA.native()));
    REQUIRE(controller.JumpFiles(1));
    REQUIRE(controller.IsCurrentFile(fileA.native()));

    std::unique_lock lock(resultMutex);
    REQUIRE(resultCv.wait_for(
        lock, std::chrono::milliseconds(500),
        [&] { return std::find(loadedFiles.begin(), loadedFiles.end(), fileC.native()) != loadedFiles.end(); }));
    lock.unlock();
    REQUIRE(controller.IsCurrentFile(fileC.native()));
}

TEST_CASE("BrowseSessionController keeps current file when invalid skip search is exhausted", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-invalid-exhausted-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    const auto fileB  = folder / LLUTILS_TEXT("b.png");
    TouchFile(fileA);
    TouchFile(fileB);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(
        std::make_unique<SelectiveResidencyProcessor>(std::set<LLUtils::native_string_type>{}), 1);
    OIV::BrowseSessionController* controllerPtr = nullptr;
    std::mutex resultMutex;
    std::condition_variable resultCv;
    std::vector<LLUtils::native_string_type> failedFiles;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            const auto result = controllerPtr->OnBrowseCandidateReady(completion);
            if (result.action == OIV::BrowseSessionController::BrowseSessionAction::ShowFailure)
            {
                {
                    std::lock_guard lock(resultMutex);
                    failedFiles.push_back(result.fileName);
                }
                resultCv.notify_all();
            }
        });
    controllerPtr = &controller;

    REQUIRE(controller.CommitCurrentFile(fileA.native()) == ResultCode::RC_Success);
    REQUIRE(controller.IsCurrentFile(fileA.native()));
    REQUIRE(controller.JumpFiles(1));
    REQUIRE(controller.IsCurrentFile(fileA.native()));

    std::unique_lock lock(resultMutex);
    REQUIRE(resultCv.wait_for(
        lock, std::chrono::milliseconds(500),
        [&] { return std::find(failedFiles.begin(), failedFiles.end(), fileB.native()) != failedFiles.end(); }));
    lock.unlock();
    REQUIRE(controller.IsCurrentFile(fileA.native()));
}

TEST_CASE("BrowseSessionController applies folder-load residency completions", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-file-session-folder-load-test");
    const auto fileA  = folder / "a.png";
    const auto fileB  = folder / "b.png";
    TouchFile(fileB);
    TouchFile(fileA);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);
    std::mutex completionMutex;
    std::condition_variable completionCv;
    std::vector<OIV::BrowseSessionController::BrowseCandidateCompletion> completions;

    OIV::BrowseSessionController controller(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [&](const OIV::BrowseSessionController::BrowseCandidateCompletion& completion)
        {
            {
                std::lock_guard lock(completionMutex);
                completions.push_back(completion);
            }
            completionCv.notify_all();
        });

    REQUIRE(controller.RequestFolderLoadResidency(folder.native()));

    OIV::BrowseSessionController::BrowseCandidateCompletion completion;
    {
        std::unique_lock lock(completionMutex);
        REQUIRE(
            completionCv.wait_for(lock, std::chrono::milliseconds(500), [&] { return completions.empty() == false; }));
        completion = completions.back();
    }

    REQUIRE(completion.folderLoad);
    REQUIRE(completion.fileName == fileA.native());
    const auto result = controller.OnFolderOpenCandidateReady(completion);
    REQUIRE(result.action == OIV::BrowseSessionController::BrowseSessionAction::DisplayImage);
    REQUIRE(controller.GetFolderFileList().GetSize() == 2);
    REQUIRE(controller.IsCurrentFile(fileA.native()));

    completion.image = nullptr;
    REQUIRE(controller.OnFolderOpenCandidateReady(completion).action ==
            OIV::BrowseSessionController::BrowseSessionAction::Ignore);
    REQUIRE(controller.IsCurrentFile(fileA.native()));
}

TEST_CASE("ImageOpenController routes folders to file session", "[AppCore]")
{
    const auto folder = MakeTempFolder("oiv-image-load-folder-route-test");
    const auto fileA  = folder / LLUTILS_TEXT("a.png");
    TouchFile(fileA);

    FakeFileWatcher watcher;
    OIV::FileSorter sorter;
    OIV::ImageResidencyCache residency(std::make_unique<CountingResidencyProcessor>(), 1);

    OIV::BrowseSessionController fileSessionController(
        &watcher, &sorter, {LLUTILS_TEXT("png")}, LLUTILS_TEXT("png"), residency,
        [](const LLUtils::native_string_type&, IMCodec::ImageSharedPtr) {},
        [](const OIV::BrowseSessionController::BrowseCandidateCompletion&) {});

    auto fakeLoader     = std::make_unique<FakeImageFileLoader>();
    auto* fakeLoaderPtr = fakeLoader.get();
    OIV::ImageOpenController controller(std::move(fakeLoader), &fileSessionController);

    const auto folderResult = controller.LoadFileOrFolder(folder.native(), IMCodec::PluginTraverseMode::NoTraverse, {});

    REQUIRE(folderResult.status == OIV::ImageLoadStatus::FolderLoadQueued);
    REQUIRE(fakeLoaderPtr->lastPath.empty());

    const auto emptyFolder       = MakeTempFolder("oiv-image-load-empty-folder-route-test");
    const auto emptyFolderResult = controller.LoadFileOrFolder(emptyFolder.native(),
                                                               IMCodec::PluginTraverseMode::NoTraverse, {});

    REQUIRE(emptyFolderResult.status == OIV::ImageLoadStatus::NoSupportedFiles);
}
