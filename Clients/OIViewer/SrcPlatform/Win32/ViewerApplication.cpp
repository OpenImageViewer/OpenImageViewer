#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Windows.h>
#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>
#include <Win32/Win32Window.h>
#include <Win32/Win32Helper.h>
#include <Win32/MonitorInfo.h>
#include <Win32/FileDialog.h>

#include <LInput/Keys/KeyCombination.h>
#include <LInput/Keys/KeyBindings.h>
#include <LInput/Buttons/Extensions/ButtonsStdExtension.h>
#include <LInput/Mouse/MouseButton.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/Logging/LogPredefined.h>
#include <LLUtils/Logging/Logger.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include "Helpers/OIVHelper.h"
#include "Helpers/ClipboardSetup.h"
#include "Helpers/MessageHelper.h"
#include "Helpers/ShellIntegrationHelper.h"
#include "Helpers/ShellCommandHandler.h"

#include "Win32/UserMessages.h"
#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include "ConfigurationLoader.h"
#include "CommandRegistry.h"
#include "ExceptionHandler.h"
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

#include "Resource.h"

namespace OIV
{
    LLUtils::native_string_type ViewerApplication::GetAppDataFolder()
    {
        return LLUtils::PlatformUtility::GetAppDataFolder() + LLUTILS_TEXT("/OIV/");
    }

    HWND ViewerApplication::FindTrayBarWindow()
    {
        using namespace Win32;

        HWND nextChild = nullptr;

        do
        {
            nextChild = FindWindowEx(nullptr, nextChild, ::Win32::Win32Window::WindowClassName, nullptr);
        } while (nextChild != nullptr && MainWindow::GetIsTrayWindow(nextChild) == false);

        return nextChild;
    }

    void ViewerApplication::Init(LLUtils::native_string_type relativeFilePath)
    {
        using namespace std;
        using namespace placeholders;

        wstring filePath = LLUtils::FileSystemHelper::ResolveFullPath(relativeFilePath);
        filePath         = std::filesystem::path(filePath).lexically_normal();

        const bool isDirectory = std::filesystem::is_directory(filePath);

        const bool isInitialFileProvided = filePath.empty() == false && isDirectory == false;
        const bool isInitialFileExists   = isInitialFileProvided && filesystem::exists(filePath);

        if (isDirectory)
            fPendingFolderLoad = filePath;

        future<bool> asyncResult;

        if (isInitialFileExists == true)
        {
            fIsTryToLoadInitialFile = true;

            // if initial file is provided, load asynchronously.
            asyncResult = async(launch::async,
                                [&]() -> bool
                                {
                                    fInitialFile = std::make_shared<OIVFileImage>(filePath);
                                    return fInitialFile->Load(&fImageLoader,
                                                              IMCodec::PluginTraverseMode::AnyPlugin |
                                                                  IMCodec::PluginTraverseMode::AnyFileType) ==
                                           RC_Success;
                                });
        }

        // initialize the windowing system of the window
        fWindow.Create();
        fWindow.SetMenuChar(false);
        fWindow.ShowStatusBar(false);
        fWindow.SetDestoryOnClose(false);
        fWindow.EnableDragAndDrop(true);
        // Set canvas background the same color as in the renderer for flicker free startup.
        // TODO: fix resize and disable background erasure of top level windows.
        fWindow.SetBackgroundColor(LLUtils::Color(45, 45, 48));
        fWindow.GetCanvasWindow().SetBackgroundColor(LLUtils::Color(45, 45, 48));

        fWindow.SetDoubleClickMode(::Win32::DoubleClickMode::Default);
        {
            using namespace ::OIV::Win32;
            fWindow.SetWindowStyles(::Win32::WindowStyle::ResizableBorder | ::Win32::WindowStyle::MaximizeButton |
                                        ::Win32::WindowStyle::MinimizeButton,
                                    true);
        }

        AutoScroll::CreateParams params = {fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL,
                                           std::bind(&ViewerApplication::OnScroll, this, std::placeholders::_1)};
        fAutoScroll                     = std::make_unique<AutoScroll>(params);

        fWindow.AddEventListener(std::bind(&ViewerApplication::HandleMessages, this, _1));
        fWindow.GetCanvasWindow().AddEventListener(std::bind(&ViewerApplication::HandleClientWindowMessages, this, _1));

        fRefreshOperation.Begin();

        fTimerNoActiveZoom.SetTargetWindow(fWindow.GetHandle());

        fTimerNoActiveZoom.SetCallback(std::bind(&ViewerApplication::DelayResamplingCallback, this));

        fTimerNavigation.SetTargetWindow(fWindow.GetHandle());
        fTimerNavigation.SetCallback(
            [this]()
            {
                using namespace Win32;
                using namespace LInput;
                const auto& mouseState = fMouseDevicesState.begin()->second;
                const int jump = (mouseState.GetButtonState(static_cast<MouseButtonType>(MouseButton::Forward)) ==
                                  ButtonState::Down)
                                     ? 1
                                 : (mouseState.GetButtonState(static_cast<MouseButtonType>(MouseButton::Back)) ==
                                    ButtonState::Down)
                                     ? -1
                                     : 0;

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
            });

        // TODO: move sequencer initialiaztion to PostInitOperations.
        fSequencerTimer.SetTargetWindow(fWindow.GetHandle());
        fSequencerTimer.SetCallback(
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
            });

        fMessageManager = std::make_unique<MessageManager>(fWindow.GetHandle(), &fLabelManager, 5,
                                                           [&]() -> void { fRefreshOperation.Queue(); });

        fRenderGateway.Initialize(fWindow.GetCanvasHandle());

        // Update oiv lib client size
        UpdateWindowSize();

        // Wait for initial file to finish loading
        bool isInitialFileLoadedSuccesfuly = false;
        if (asyncResult.valid())
        {
            asyncResult.wait();
            isInitialFileLoadedSuccesfuly = asyncResult.get();
        }

        // If there is no initial file or the file has failed to load, show the window now, otherwise show the window
        // after the image has rendered completely at the method FinalizeImageLoad.
        fWindow.SetVisible(!isInitialFileLoadedSuccesfuly);

        // If initial file is provided but doesn't exist
        if (isInitialFileProvided && !isInitialFileExists)
        {
            using namespace std::string_literals;
            SetUserMessage(LLUTILS_TEXT("Can not load the file: "s) + filePath +
                               LLUTILS_TEXT(", it doesn't exist"s),
                           static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
        }

        fRefreshOperation.End(!isInitialFileLoadedSuccesfuly);

        if (isInitialFileLoadedSuccesfuly)
        {
            LoadOivImage(fInitialFile);
            fInitialFile.reset();
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
        LLUtils::Logger::GetSingleton().AddLogTarget(&mLogFile);

        fTimerTopMostRetention.SetTargetWindow(fWindow.GetHandle());
        fTimerTopMostRetention.SetCallback([this]() { ProcessTopMost(); });

        fTimerSlideShow.SetTargetWindow(fWindow.GetHandle());
        fTimerSlideShow.SetCallback(
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
            });

        fDoubleTap.callback = [this]()
        {
            fWindow.SetAlwaysOnTop(true);
            fTopMostCounter = 3;
            SetTopMostUserMesage();
            fTimerTopMostRetention.SetInterval(1000);
        };

        const ImageFormatCatalog imageFormatCatalog = ImageFormatCatalogPolicy::Build(
            fImageLoader.GetImageCodec().GetPluginsInfo());

        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters readFilters;
        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters writeFilters;

        for (const ImageFormatFilter& filter : imageFormatCatalog.readFilters)
            readFilters.push_back({filter.description, filter.extensions});

        for (const ImageFormatFilter& filter : imageFormatCatalog.writeFilters)
            writeFilters.push_back({filter.description, filter.extensions});

        fKnownFileTypesSet          = imageFormatCatalog.knownFileTypesSet;
        fKnownFileTypes             = imageFormatCatalog.knownFileTypes;
        fDefaultSaveFileExtension   = imageFormatCatalog.defaultSaveFileExtension;
        fDefaultSaveFileFormatIndex = imageFormatCatalog.defaultSaveFileFormatIndex;

        fOpenComDlgFilters = {readFilters};
        fSaveComDlgFilters = {writeFilters};

        fFileWatcher.FileChangedEvent.Add(std::bind(&ViewerApplication::OnFileChanged, this, std::placeholders::_1));

        fBrowseSessionController = std::make_unique<BrowseSessionController>(
            &fFileWatcher, &fFileSorter, fKnownFileTypesSet, fKnownFileTypes, fImageResidencyCache,
            [this](const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
            {
                if (!fIsShuttingDown)
                {
                    fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                           InterThreadMessages::FileIndexResidencyReady),
                                       FileIndexResidencyReadyData{fileName, image});
                }
            },
            [this](const BrowseSessionController::BrowseCandidateCompletion& completion)
            {
                if (!fIsShuttingDown)
                {
                    fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(
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

        // renderer took over on the window, no need to erase background.
        fWindow.GetCanvasWindow().SetEraseBackground(false);

        fContextMenuTimer.SetTargetWindow(fWindow.GetHandle());
        fContextMenuTimer.SetCallback(std::bind(&ViewerApplication::OnContextMenuTimer, this));
        fContextMenu = std::make_unique<ContextMenu<MenuItemData>>(fWindow.GetHandle());

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

        fNotificationIconID = fNotificationIcons.AddIcon(MAKEINTRESOURCE(IDI_APP_ICON),
                                                         LLUTILS_TEXT("Open Image Viewer"));
        fNotificationIcons.OnNotificationIconEvent.Add(
            std::bind(&ViewerApplication::OnNotificationIcon, this, std::placeholders::_1));

        fNotificationContextMenu = std::make_unique<ContextMenu<int>>(fWindow.GetHandle());
        fNotificationContextMenu->AddItem(OIV_TEXT("Quit"), int{});

        using namespace LInput;
        fRawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls,
                            RawInput::GenericDesktopControlsUsagePage::Mouse, RawInput::Flags::EnableBackground);

        fRawInput.OnInput.Add(std::bind(&ViewerApplication::OnRawInput, this, std::placeholders::_1));
        fRawInput.Enable(true);

        fMouseClickEventHandler.OnMouseClickEvent.Add(
            std::bind(&ViewerApplication::OnMouseMultiClick, this, std::placeholders::_1));

        LoadSettings();

        if (fReloadSettingsFileIfChanged)
            fCOnfigurationFolderID = fFileWatcher.AddFolder(LLUtils::PlatformUtility::GetExeFolder() +
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

    void ViewerApplication::Run()
    {
        bool shouldQuit = false;
        while (!shouldQuit)
        {
            DWORD count  = 1;
            DWORD result = MsgWaitForMultipleObjects(count, &fEventSync.GetEventHandle(), FALSE, INFINITE, QS_ALLINPUT);

            if (result < count)
            {
                fEventSync.ProcessData();
            }
            else if (result == WAIT_FAILED)
            {
                std::cerr << "Wait failed! Error: " << GetLastError() << std::endl;
                break;
            }
            else
            {
                // Handle Windows messages
                shouldQuit = ::Win32::Win32Helper::ProcessApplicationMessage();
            }
        }
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

        if (fWindow.GetFullScreenState() != ::Win32::FullSceenState::MultiScreen) [[likely]]
        {
            canvasCenter = PointF64(fWindow.GetCanvasSize()) / 2.0;
        }
        else [[unlikely]]
        {
            RECT primaryMonitorCoords =
                ::Win32::MonitorInfo::GetSingleton().GetPrimaryMonitor(false).monitorInfo.rcMonitor;
            RECT boundingArea = ::Win32::MonitorInfo::GetSingleton().getBoundingMonitorArea();

            using point_type = PointF64::point_type;
            auto leftDelta   = primaryMonitorCoords.left - boundingArea.left;
            auto topDelta    = primaryMonitorCoords.top - boundingArea.top;

            const LLUtils::PointF64 primaryScreenOffset = LLUtils::PointF64(static_cast<point_type>(leftDelta),
                                                                            static_cast<point_type>(topDelta));

            const LLUtils::PointF64 primaryScreenSize = LLUtils::PointF64(
                static_cast<point_type>(primaryMonitorCoords.right - primaryMonitorCoords.left),
                static_cast<point_type>(primaryMonitorCoords.bottom - primaryMonitorCoords.top));

            canvasCenter = primaryScreenOffset + primaryScreenSize / 2.0;
        }
        return canvasCenter;
    }

    LLUtils::PointF64 ViewerApplication::ResolveOffset(const LLUtils::PointF64& point)
    {
        using namespace LLUtils;
        return ViewTransformController::ResolveOffset(point, static_cast<PointF64>(fWindow.GetCanvasSize()),
                                                      GetImageSize(ImageSizeType::Visible), fImageMargins);
    }

}  // namespace OIV
