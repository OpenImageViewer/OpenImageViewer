#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>
#include <algorithm>
#include <cmath>

#include "ViewerApplication.h"

#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>
#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/WindowExtensions.hpp>
#endif

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCountPolicy.h>
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/OIVImageHelper.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/PixelHelper.h>
#include <ImageUtil/ImageUtil.h>
#include "InterThreadMessages.h"

namespace OIV
{
    void ViewerApplication::Init(LLUtils::native_string_type relativeFilePath, const RendererOptions& rendering)
    {
        using namespace std;
        using namespace placeholders;

        LLUtils::native_string_type filePath = LLUtils::FileSystemHelper::ResolveFullPath(relativeFilePath);
        filePath                             = std::filesystem::path(filePath).lexically_normal();

        const bool isDirectory = std::filesystem::is_directory(filePath);

        const bool isInitialFileProvided = filePath.empty() == false && isDirectory == false;
        const bool isInitialFileExists   = isInitialFileProvided && filesystem::exists(filePath);

        if (isDirectory)
            fPendingFolderLoad = filePath;

        // Declare metadata before the future so the worker finishes before it is destroyed
        // during unwinding. The UI thread reads it only after future::get completes.
        IMCodec::ItemMetaDataSharedPtr initialMetaData;
        future<IMCodec::ImageSharedPtr> asyncResult;

        if (isInitialFileExists == true)
        {
            fIsTryToLoadInitialFile = true;

            // Decode pixels and metadata while the UI thread initializes the renderer.
            // No OIV wrapper is constructed on the worker.
            asyncResult = async(launch::async,
                                [this, &filePath, &initialMetaData]()
                                {
                                    return DecodeFileImage(fImageLoader, filePath, initialMetaData,
                                                           IMCodec::PluginTraverseMode::AnyPlugin |
                                                               IMCodec::PluginTraverseMode::AnyFileType);
                                });
        }

        // initialize the windowing system of the window
        // The historical 1200x800 native window was calibrated at 125% scaling. LWS uses 96-DPI logical units, so
        // this client estimate preserves that reference footprint while remaining portable.
        LWS::LogicalSize initialClientSize{946, 602};
        if (const auto monitor = fPlatform.GetPrimaryMonitor(); monitor.has_value())
        {
            double workWidth  = monitor->workRect.GetWidth();
            double workHeight = monitor->workRect.GetHeight();
#ifdef LWS_HAS_WIN32_BACKEND
            workWidth /= monitor->contentScale.x;
            workHeight /= monitor->contentScale.y;
#endif
            constexpr double edgeMargins = 64.0;
            const double scale           = std::min({1.0, std::max(workWidth - edgeMargins, 1.0) / initialClientSize.x,
                                                     std::max(workHeight - edgeMargins, 1.0) / initialClientSize.y});
            initialClientSize            = {
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.x * scale))),
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.y * scale))),
            };
        }
        const LWS::WindowConfig windowConfig{
            .clientSize         = initialClientSize,
            .styles             = LWS::WindowStyleFlags(WindowChromeStyles),
            .backgroundColor    = LLUtils::Color(45, 45, 48),
            .dragAndDropEnabled = fPlatform.Supports(LWS::PlatformFeature::DragAndDrop).value_or(false),
        };
        if (fWindow.Create(windowConfig) != LWS::Result::Success)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to create the main window");
        std::ignore = fWindow.GetWindow().Center(LWS::CenterTarget::PrimaryMonitor);
        fMonitorProvider.UpdateFromWindow(fWindow.GetWindow());
#ifdef LWS_HAS_WIN32_BACKEND
        std::ignore = LWS::Win32::SetMenuChar(fWindow.GetWindow(), false);
#endif
        fWindow.ShowStatusBar(false);
        std::ignore = fWindow.GetCanvasWindow().SetBackgroundColor(LLUtils::Color(45, 45, 48));

        AutoScroll::CreateParams params = {&fWindow.GetWindow(),
                                           std::bind(&ViewerApplication::OnScroll, this, std::placeholders::_1)};
        fAutoScroll                     = std::make_unique<AutoScroll>(params);

        auto windowConnection = fWindow.GetWindow().Listen(
            [this](const LWS::AnyEvent& eventData)
            {
                if (std::holds_alternative<LWS::EventWindowDestroyed>(eventData))
                    fPlatform.RequestQuit();
                return HandleEventCallback([&]() { return HandleMessages(eventData); }) ? LWS::EventResponse::Handled
                                                                                        : LWS::EventResponse::Unhandled;
            });
        auto canvasConnection = fWindow.GetCanvasWindow().Listen(
            [this](const LWS::AnyEvent& eventData)
            {
                return HandleEventCallback([&]() { return HandleClientWindowMessages(eventData); })
                           ? LWS::EventResponse::Handled
                           : LWS::EventResponse::Unhandled;
            });
        if (!windowConnection.has_value() || !canvasConnection.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to register window listeners");
        fWindowConnection = std::move(*windowConnection);
        fCanvasConnection = std::move(*canvasConnection);

        fRefreshOperation.Begin();

        std::ignore = fTimerNoActiveZoom.SetTargetWindow(&fWindow.GetWindow());

        fTimerNoActiveZoom.SetCallback(MakeSafeCallback([this]() { DelayResamplingCallback(); }));

        std::ignore = fTimerNavigation.SetTargetWindow(&fWindow.GetWindow());
        fTimerNavigation.SetCallback(MakeSafeCallback(
            [this]()
            {
                const int jump = GetRawNavigationDirection();

                if (jump != 0 &&
                    fLastImageLoadTimeStamp.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds) > fQuickBrowseDelay)
                {
                    fLastImageLoadTimeStamp.Start();
                    fLastImageLoadTimeStamp.Stop();

                    if (JumpFiles(jump) == false)
                    {
                        fLastImageLoadTimeStamp.Start();
                    }
                }
            }));

        // TODO: move sequencer initialiaztion to PostInitOperations.
        std::ignore = fSequencerTimer.SetTargetWindow(&fWindow.GetWindow());
        fSequencerTimer.SetCallback(MakeSafeCallback(
            [this]()
            {
                auto currentImage = fImageState.GetOpenedImage()->GetImage()->GetSubImage(fCurrentFrame);
                fImageState.SetImageChainRoot(
                    std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, currentImage));

                fSequencerTimer.SetInterval(SequencerPolicy::FrameIntervalMs(
                    currentImage->GetAnimationData().delayMilliseconds, fCurrentSequencerSpeed));
                fCurrentFrame = SequencerPolicy::NextFrame(fCurrentFrame,
                                                           fImageState.GetOpenedImage()->GetImage()->GetNumSubImages());
                RefreshImage();
            }));

        fMessageManager = std::make_unique<MessageManager>(fWindow.GetWindow(), &fLabelManager, 5,
                                                           [&]() -> void { fRefreshOperation.Queue(); });

        // Stop LWS buffer attachments before the renderer takes ownership of the canvas. On Wayland,
        // a background buffer committed after Vulkan enables explicit sync has no acquire/release points.
        std::ignore = fWindow.GetCanvasWindow().SetEraseBackground(false);
        InitializeRenderer(rendering);

        // Update oiv lib client size
        UpdateWindowSize();
        fWindow.ShowCanvas();

        IMCodec::ImageSharedPtr initialImage;
        if (asyncResult.valid())
            initialImage = asyncResult.get();
        const bool isInitialFileLoadedSuccesfuly = initialImage != nullptr;

        // If there is no initial file or the file has failed to load, show the window now, otherwise show the window
        // after the image has rendered completely at the method FinalizeImageLoad.
        std::ignore = fWindow.GetWindow().SetVisible(!isInitialFileLoadedSuccesfuly);

        // If initial file is provided but doesn't exist
        if (isInitialFileProvided && !isInitialFileExists)
        {
            using namespace std::string_literals;
            SetUserMessage(LLUTILS_TEXT("Can not load the file: "s) + filePath + LLUTILS_TEXT(", it doesn't exist"s),
                           static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
        }

        fRefreshOperation.End(!isInitialFileLoadedSuccesfuly);

        if (isInitialFileLoadedSuccesfuly)
        {
            // Registration happens here on the UI thread, after the renderer is ready.
            auto file = std::make_shared<OIVFileImage>(filePath, std::move(initialImage));
            file->SetMetaData(std::move(initialMetaData));
            LoadOivImage(std::move(file));
        }
    }

    IMCodec::ImageSharedPtr ViewerApplication::GetImageByIndex(int32_t index)
    {
        using namespace IMCodec;
        auto openedImage = fImageState.GetOpenedImage()->GetImage();

        const auto isMainAnActualImage = SubImagePolicy::IncludeMainImage(openedImage->GetItemType());
        const auto actualIndex         = SubImagePolicy::ActualImageIndexFromDisplayIndex(index, isMainAnActualImage);

        if (actualIndex == SubImagePolicy::MainImageIndex)
        {
            return openedImage;
        }
        else
        {
            return openedImage->GetSubImage(actualIndex);
        }
    }

    void ViewerApplication::PostInitOperations()
    {
        mLogFile.Register();

        std::ignore = fTimerTopMostRetention.SetTargetWindow(&fWindow.GetWindow());
        fTimerTopMostRetention.SetCallback(MakeSafeCallback([this]() { ProcessTopMost(); }));

        std::ignore = fTimerSlideShow.SetTargetWindow(&fWindow.GetWindow());
        fTimerSlideShow.SetCallback(MakeSafeCallback(
            [this]()
            {
                SetSlideShowEnabled(false);

                if (fBrowseSessionController == nullptr)
                    return;

                const auto& fileList = fBrowseSessionController->GetFolderFileList();
                bool foundFile       = JumpFiles(1) ||
                                       (fSlideshowPolicy.ShouldWrap(fileList.GetCurrentIndex(), fileList.GetSize()) &&
                                        JumpFiles(FolderFileList::IndexStart));

                SetSlideShowEnabled(foundFile);
            }));

        fDoubleTap.callback = [this]()
        {
            std::ignore     = fWindow.GetWindow().SetAlwaysOnTop(true);
            fTopMostCounter = 3;
            SetTopMostUserMesage();
            fTimerTopMostRetention.SetInterval(1000);
        };

        const ImageFormatCatalog imageFormatCatalog = ImageFormatCatalogPolicy::Build(
            fImageLoader.GetImageCodec().GetPluginsInfo());

        LWS::FileDialogFilterBuilder::ListFileDialogFilters readFilters;
        LWS::FileDialogFilterBuilder::ListFileDialogFilters writeFilters;

        for (const ImageFormatFilter& filter : imageFormatCatalog.readFilters)
            readFilters.push_back({filter.description, filter.extensions});

        for (const ImageFormatFilter& filter : imageFormatCatalog.writeFilters)
            writeFilters.push_back({filter.description, filter.extensions});

        fKnownFileTypesSet          = imageFormatCatalog.knownFileTypesSet;
        fKnownFileTypes             = imageFormatCatalog.knownFileTypes;
        fDefaultSaveFileExtension   = imageFormatCatalog.defaultSaveFileExtension;
        fDefaultSaveFileFormatIndex = imageFormatCatalog.defaultSaveFileFormatIndex;

        fOpenComDlgFilters = LWS::FileDialogFilterBuilder(readFilters);
        fSaveComDlgFilters = LWS::FileDialogFilterBuilder(writeFilters);

        if (fFileWatcher != nullptr)
        {
            fFileWatcher->GetFileChangedEvent().Add(
                std::bind(&ViewerApplication::OnFileChanged, this, std::placeholders::_1));
        }

        fBrowseSessionController = std::make_unique<BrowseSessionController>(
            fFileWatcher.get(), &fFileSorter, fKnownFileTypesSet, fKnownFileTypes, fImageResidencyCache,
            [this](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
            {
                if (!fIsShuttingDown)
                {
                    QueueUiCompletion(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                          InterThreadMessages::FileIndexResidencyReady),
                                      FileIndexResidencyReadyData{fileName, image});
                }
            },
            [this](const BrowseSessionController::BrowseCandidateCompletion& completion)
            {
                if (!fIsShuttingDown)
                {
                    QueueUiCompletion(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                          InterThreadMessages::CandidateResidencyReady),
                                      CandidateResidencyReadyData{completion});
                }
            });
        fImageOpenController->SetBrowseSessionController(fBrowseSessionController.get());

        if (IsOpenedImageIsAFile())
            (void) fBrowseSessionController->CommitCurrentFile(GetOpenedFileName());
        UpdateTitle();

        AddCommandsAndKeyBindings();

        fWindow.GetImageControl().GetImageList().ImageSelectionChanged.Add(
            std::bind(&ViewerApplication::OnImageSelectionChanged, this, std::placeholders::_1));

        std::ignore = fContextMenuTimer.SetTargetWindow(&fWindow.GetWindow());
        fContextMenuTimer.SetCallback(MakeSafeCallback([this]() { OnContextMenuTimer(); }));
        fContextMenu = std::make_unique<ContextMenu<MenuItemData>>(fWindow.GetWindow());

        fContextMenu->AddItem(LLUTILS_TEXT("Open"), MenuItemData{"cmd_open_file", ""});
        fContextMenu->AddItem(LLUTILS_TEXT("Open containing folder"),
                              MenuItemData{"cmd_shell", "cmd=containingFolder"});
        fContextMenu->AddItem(LLUTILS_TEXT("Open in new window"), MenuItemData{"cmd_shell", "cmd=newWindow"});
        fContextMenu->AddItem(LLUTILS_TEXT("Open in photoshop"), MenuItemData{"cmd_shell", "cmd=openPhotoshop"});
        fContextMenu->AddItem(LLUTILS_TEXT("Quit"), MenuItemData{"cmd_view_state", "type=quit"});

        fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"),
                                 fImageState.GetOpenedImage() != nullptr &&
                                     fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File);
        fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"),
                                 fImageState.GetOpenedImage() != nullptr &&
                                     fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File);

        InitializeNotificationIcons();

        fNotificationContextMenu = std::make_unique<ContextMenu<int>>(fWindow.GetWindow());
        fNotificationContextMenu->AddItem(LLUTILS_TEXT("Quit"), int{});

        InitializeRawInput();

        LoadSettings();

        if (fReloadSettingsFileIfChanged && fFileWatcher != nullptr)
            fCOnfigurationFolderID = fFileWatcher->AddFolder(LLUtils::PlatformUtility::GetExeFolder() +
                                                             LLUTILS_TEXT("./Resources/Configuration/."));

        if (fPendingFolderLoad.empty() == false)
        {
            LoadFileOrFolder(fPendingFolderLoad,
                             IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType);
            fPendingFolderLoad.clear();
        }

        if (IsImageOpen() == false)
        {
            ShowWelcomeMessage();
            UpdateTitle();
        }

        ClipboardSetup::RegisterDefaultFormats(fClipboardHelper);
    }

    LLUtils::PointF64 ViewerApplication::GetImageSize(ImageSizeType imageSizeType)
    {
        using namespace LLUtils;
        switch (imageSizeType)
        {
            case ImageSizeType::Original:
                return fImageState.GetImage(ImageChainStage::SourceImage) != nullptr
                           ? PointF64(fImageState.GetImage(ImageChainStage::SourceImage)->GetImage()->GetDimensions())
                           : PointF64(0, 0);
            case ImageSizeType::Transformed:
                return static_cast<PointF64>(
                    fImageState.GetImage(ImageChainStage::Deformed)->GetImage()->GetDimensions());
            case ImageSizeType::Visible:
                return fImageState.GetVisibleSize();

            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    LLUtils::PointI32 ViewerApplication::SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen)
    {
        return SelectionWorkflowPolicy::SnapToImagePixels(pointOnScreen, GetScale(), GetOffset());
    }

    LLUtils::RectI32 ViewerApplication::ClientToImageRounded(LLUtils::RectI32 clientRect) const
    {
        return static_cast<LLUtils::RectI32>(ClientToImage(clientRect).Round());
    }

    LLUtils::PointF64 ViewerApplication::GetOffset() const
    {
        return fImageState.GetOffset();
    }

    LLUtils::PointF64 ViewerApplication::ImageToClient(LLUtils::PointF64 imagepos) const
    {
        return ViewTransformController::ImageToClient(imagepos, GetScale(), GetOffset());
    }

    LLUtils::RectF64 ViewerApplication::ImageToClient(LLUtils::RectF64 clientRect) const
    {
        return ViewTransformController::ImageToClient(clientRect, GetScale(), GetOffset());
    }

    LLUtils::PointF64 ViewerApplication::ClientToImage(LLUtils::PointI32 clientPos) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::PointF64>(clientPos), GetScale(),
                                                      GetOffset());
    }

    LLUtils::RectF64 ViewerApplication::ClientToImage(LLUtils::RectI32 clientRect) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::RectF64>(clientRect), GetScale(),
                                                      GetOffset());
    }

    LLUtils::PointF64 ViewerApplication::GetCanvasCenter()
    {
        using namespace LLUtils;

        PointF64 canvasCenter;

        if (fWindow.GetWindow().GetWindowMode() != LWS::WindowMode::FullscreenAllMonitors) [[likely]]
        {
            const LWS::PixelSize size = fWindow.GetCanvasPixelSize();
            canvasCenter              = PointF64(size.x, size.y) / 2.0;
        }
        else [[unlikely]]
        {
            const auto primaryMonitor   = fPlatform.GetPrimaryMonitor(false)->monitorRect;
            const auto boundingArea     = *fPlatform.GetBoundingMonitorArea();
            const auto primaryMonitorP0 = primaryMonitor.GetCorner(TopLeft);
            const auto boundingAreaP0   = boundingArea.GetCorner(TopLeft);

            using point_type     = PointF64::point_type;
            const auto leftDelta = primaryMonitorP0.x - boundingAreaP0.x;
            const auto topDelta  = primaryMonitorP0.y - boundingAreaP0.y;

            const LLUtils::PointF64 primaryScreenOffset = LLUtils::PointF64(static_cast<point_type>(leftDelta),
                                                                            static_cast<point_type>(topDelta));

            const LLUtils::PointF64 primaryScreenSize = LLUtils::PointF64(
                static_cast<point_type>(primaryMonitor.GetWidth()),
                static_cast<point_type>(primaryMonitor.GetHeight()));

            canvasCenter = primaryScreenOffset + primaryScreenSize / 2.0;
        }
        return canvasCenter;
    }

    LLUtils::PointF64 ViewerApplication::ResolveOffset(const LLUtils::PointF64& point)
    {
        using namespace LLUtils;
        const LWS::PixelSize size = fWindow.GetCanvasPixelSize();
        return ViewTransformController::ResolveOffset(point, PointF64(size.x, size.y),
                                                      GetImageSize(ImageSizeType::Visible), fImageMargins);
    }

}  // namespace OIV
