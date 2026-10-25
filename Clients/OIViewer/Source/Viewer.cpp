#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>
#include <algorithm>
#include <cmath>

#include "Viewer.h"

#include <Version.h>

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

#include <ImageProcessing/FileImage.h>
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <ImageUtil/ImageUtil.h>
#include <OIVAppCore/ViewerCompletion.h>

namespace OIV
{
    void Viewer::Init(LLUtils::native_string_type relativeFilePath, const RendererOptions& rendering,
                      const LWS::WindowConfig& windowConfig)
    {
        using namespace std;
        using namespace placeholders;

        LLUtils::native_string_type filePath = LLUtils::FileSystemHelper::ResolveFullPath(relativeFilePath);
        filePath                             = std::filesystem::path(filePath).lexically_normal();

        const bool isDirectory = std::filesystem::is_directory(filePath);

        const bool isInitialFileProvided = filePath.empty() == false && isDirectory == false;
        const bool isInitialFileExists   = isInitialFileProvided && filesystem::exists(filePath);

        if (isDirectory)
            fSession.fPendingFolderLoad = filePath;

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
                                    return DecodeFileImage(fSession.fImageLoader, filePath, initialMetaData,
                                                           IMCodec::PluginTraverseMode::AnyPlugin |
                                                               IMCodec::PluginTraverseMode::AnyFileType);
                                });
        }

        if (fWindow.Create(windowConfig) != LWS::Result::Success)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to create the main window");
        if (fShell.created)
            fShell.created();
        fMonitorProvider.UpdateFromWindow(fWindow.GetWindow());
#ifdef LWS_HAS_WIN32_BACKEND
        std::ignore = LWS::Win32::SetMenuChar(fWindow.GetWindow(), false);
#endif
        fWindow.ShowStatusBar(false);
        std::ignore = fWindow.GetCanvasWindow().SetBackgroundColor(LLUtils::Color(45, 45, 48));

        AutoScroll::CreateParams params = {&fWindow.GetWindow(),
                                           std::bind(&Viewer::OnScroll, this, std::placeholders::_1)};
        fAutoScroll                     = std::make_unique<AutoScroll>(params);

        auto windowConnection = fWindow.GetWindow().Listen(
            [this](const LWS::AnyEvent& eventData)
            {
                if (std::holds_alternative<LWS::EventWindowDestroying>(eventData))
                    Stop();
                if (std::holds_alternative<LWS::EventWindowDestroyed>(eventData) && fShell.destroyed)
                    fShell.destroyed();
                return HandleEventCallback([&]() { return HandleMessages(eventData); }) ? LWS::EventResponse::Handled
                                                                                        : LWS::EventResponse::Unhandled;
            });
        auto canvasConnection = fWindow.GetCanvasWindow().Listen(
            [this](const LWS::AnyEvent& eventData)
            {
                if (std::holds_alternative<LWS::EventWindowDestroying>(eventData))
                    Stop();
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
                // The session deadline, not a possibly stale native tick, decides whether to advance.
                const auto step = fSession.AdvanceAnimation();
                fSequencerTimer.SetInterval(step.intervalMs);
                if (step.changed)
                    RefreshImage();
            }));

        fMessageManager = std::make_unique<MessageManager>(fWindow.GetWindow(), &fLabelManager, 5,
                                                           MakeSafeCallback([this]() { fRefreshOperation.Queue(); }));

        // Stop LWS buffer attachments before the renderer takes ownership of the canvas. On Wayland,
        // a background buffer committed after Vulkan enables explicit sync has no acquire/release points.
        std::ignore = fWindow.GetCanvasWindow().SetEraseBackground(false);
        InitializeRenderer(rendering);
        // A new renderer starts with backend defaults, while color correction survives in the session.
        UpdateExposure();
        UpdateRenderViewParams();
        fLabelManager.InitializeRenderer(*fRenderer);

        // Capture the canvas extent for the first presentation.
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
            LoadOivImage(ViewerImage{.pixels   = std::move(initialImage),
                                     .metadata = std::move(initialMetaData),
                                     .source   = ImageSource::File,
                                     .path     = std::filesystem::path(filePath).lexically_normal().native()});
        }
        else if (IsImageOpen())
        {
            // A new surface observes the existing session. Reopening the image here would reset its source
            // generation, animation position, and selection, and invalidate otherwise reusable CPU caches.
            fSession.fImageState.Refresh();
            UpdateMainRenderImages();
            fWindow.SetShowImageControl(IsSubImagesVisible());
            UnloadWelcomeMessage();
            fRestoringSession = true;
            LoadSubImages(true);
            fRestoringSession = false;
            LoadImageSpaceSelection();
            UpdateOpenImageUI();
            SetResamplingEnabled(true);
            const auto& pixels = fSession.fImageState.GetOpenedImage()->pixels;
            if (pixels->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame && pixels->GetNumSubImages())
                fSequencerTimer.SetInterval(std::max(1U, fSession.fSequencerPolicy.RemainingFrameMs()));
        }
        fSession.Attach(MakeSafeCallback([this] { DrainUiCompletions(); }));
        fSessionAttached = true;
    }

    IMCodec::ImageSharedPtr Viewer::GetImageByIndex(int32_t index)
    {
        using namespace IMCodec;
        auto openedImage = fSession.fImageState.GetOpenedImage()->pixels;

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

    void Viewer::PostInitOperations()
    {
        std::ignore = fTimerSlideShow.SetTargetWindow(&fWindow.GetWindow());
        fTimerSlideShow.SetCallback(MakeSafeCallback(
            [this]()
            {
                SetSlideShowEnabled(false);

                if (fSession.fBrowseSessionController == nullptr)
                    return;

                const auto& fileList = fSession.fBrowseSessionController->GetFolderFileList();
                bool foundFile       = JumpFiles(1) || (fSession.fSlideshowPolicy.ShouldWrap(fileList.GetCurrentIndex(),
                                                                                             fileList.GetSize()) &&
                                                        JumpFiles(FolderFileList::IndexStart));

                SetSlideShowEnabled(foundFile);
            }));

        fDoubleTap.callback = [this]
        {
            if (fShell.topMost)
                fShell.topMost();
        };

        const ImageFormatCatalog imageFormatCatalog = ImageFormatCatalogPolicy::Build(
            fSession.fImageLoader.GetImageCodec().GetPluginsInfo());

        LWS::ListFileDialogFilters readFilters;
        LWS::ListFileDialogFilters writeFilters;

        for (const ImageFormatFilter& filter : imageFormatCatalog.readFilters)
            readFilters.push_back({filter.description, filter.extensions});

        for (const ImageFormatFilter& filter : imageFormatCatalog.writeFilters)
            writeFilters.push_back({filter.description, filter.extensions});

        fSession.fKnownFileTypesSet = imageFormatCatalog.knownFileTypesSet;
        fSession.fKnownFileTypes    = imageFormatCatalog.knownFileTypes;
        fDefaultSaveFileExtension   = imageFormatCatalog.defaultSaveFileExtension;
        fDefaultSaveFileFormatIndex = imageFormatCatalog.defaultSaveFileFormatIndex;

        fOpenComDlgFilters = std::move(readFilters);
        fSaveComDlgFilters = std::move(writeFilters);

        fSession.InitializeBrowsing();

        UpdateTitle();

        AddCommandsAndKeyBindings();

        fImageSelectionSubscription = fWindow.GetImageControl().GetImageList().ImageSelectionChanged.Subscribe(
            std::bind(&Viewer::OnImageSelectionChanged, this, std::placeholders::_1));

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
                                 fSession.fImageState.GetOpenedImage() != nullptr &&
                                     fSession.fImageState.GetOpenedImage()->source == ImageSource::File);
        fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"),
                                 fSession.fImageState.GetOpenedImage() != nullptr &&
                                     fSession.fImageState.GetOpenedImage()->source == ImageSource::File);

        InitializeRawInput();

        LoadSettings();

        if (fReloadSettingsFileIfChanged && fSession.fFileWatcher != nullptr && fSession.fConfigurationFolderID == 0)
            fSession.fConfigurationFolderID = fSession.fFileWatcher->AddFolder(fSettingsFile.parent_path().native());

        if (fSession.fPendingFolderLoad.empty() == false)
        {
            LoadFileOrFolder(fSession.fPendingFolderLoad,
                             IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType);
            fSession.fPendingFolderLoad.clear();
        }

        if (IsImageOpen() == false)
        {
            ShowWelcomeMessage();
            UpdateTitle();
        }

        ClipboardSetup::RegisterDefaultFormats(fClipboardHelper);
        if (fShell.ready)
            fShell.ready();
    }

    LLUtils::PointF64 Viewer::GetImageSize(ImageSizeType type)
    {
        return fSession.ImageSize(type);
    }

    LLUtils::PointI32 Viewer::SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen)
    {
        return SelectionWorkflowPolicy::SnapToImagePixels(pointOnScreen, GetScale(), GetOffset());
    }

    LLUtils::RectI32 Viewer::ClientToImageRounded(LLUtils::RectI32 clientRect) const
    {
        return static_cast<LLUtils::RectI32>(ClientToImage(clientRect).Round());
    }

    LLUtils::PointF64 Viewer::GetOffset() const
    {
        return fSession.fImageState.GetOffset();
    }

    LLUtils::PointF64 Viewer::ImageToClient(LLUtils::PointF64 imagepos) const
    {
        return ViewTransformController::ImageToClient(imagepos, GetScale(), GetOffset());
    }

    LLUtils::RectF64 Viewer::ImageToClient(LLUtils::RectF64 clientRect) const
    {
        return ViewTransformController::ImageToClient(clientRect, GetScale(), GetOffset());
    }

    LLUtils::PointF64 Viewer::ClientToImage(LLUtils::PointI32 clientPos) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::PointF64>(clientPos), GetScale(),
                                                      GetOffset());
    }

    LLUtils::RectF64 Viewer::ClientToImage(LLUtils::RectI32 clientRect) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::RectF64>(clientRect), GetScale(),
                                                      GetOffset());
    }

    LLUtils::PointF64 Viewer::GetCanvasCenter()
    {
        using namespace LLUtils;

        PointF64 canvasCenter;

        if (fWindow.GetWindowMode() != LWS::WindowMode::FullscreenAllMonitors) [[likely]]
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

}  // namespace OIV
