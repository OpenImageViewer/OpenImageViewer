#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>
#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/WindowExtensions.hpp>
#elif defined(LWS_HAS_WAYLAND_BACKEND)
    #include <LWS/Wayland/WindowExtensions.hpp>
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
#include <OIVAppCore/MessageFormatter.h>
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
#include "ViewerMouseInput.h"

namespace OIV
{
    ViewerApplication::~ViewerApplication()
    {
        fIsShuttingDown = true;
        fRefreshTimer.Enable(false);
        {
            // Producers snapshot this token under the same lock before posting UI work.
            const std::scoped_lock lock(fUiCompletionMutex);
            fUiLifetime.reset();
        }
        if (fCountingColorsThread.joinable())
            fCountingColorsThread.join();
    }

    void ViewerApplication::SetImageInfoVisible(bool visible)
    {
        if (visible != fImageInfoVisible)
        {
            fImageInfoVisible = visible;

            if (fImageInfoVisible == true)
            {
                ShowImageInfo();
            }
            else
            {
                OIVTextImage* text = fLabelManager.GetTextLabel("imageInfo");
                if (text != nullptr)
                {
                    fLabelManager.Remove("imageInfo");
                    fRefreshOperation.Queue();
                }
            }
        }
    }

    bool ViewerApplication::GetImageInfoVisible() const
    {
        return fImageInfoVisible;
    }

    void ViewerApplication::NetSettingsCallback_(ItemChangedArgs* args)
    {
        reinterpret_cast<ViewerApplication*>(args->userData)->NetSettingsCallback(args);
    }

    void ViewerApplication::NetSettingsCallback(ItemChangedArgs* args)
    {
        OnSettingChange(LLUtils::StringUtility::ToNativeString(std::wstring(args->key)),
                        LLUtils::StringUtility::ToNativeString(std::wstring(args->val)));
    }

    LLUtils::native_string_type ViewerApplication::GetLogFilePath()
    {
        return GetAppDataFolder() + LLUtils::StringUtility::ToNativeString(FormatFullVersion(CurrentVersion)) +
               LLUTILS_TEXT("/oiv.log");
    }

    void ViewerApplication::HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args,
                                            LLUtils::native_string_type seperatedCallStack)
    {
        LLUtils::native_stringstream ss;
        LLUtils::native_string_type source = isFromLibrary ? LLUTILS_TEXT("OIV library") : LLUTILS_TEXT("OIV viewer");
        const LLUtils::native_string_type introMessage = LLUtils::Exception::ExceptionErrorCodeToString(
                                                             args.errorCode) +
                                                         LLUTILS_TEXT(" exception has occured at ") +
                                                         args.functionName + LLUTILS_TEXT(" at ") + source +
                                                         LLUTILS_TEXT(".\nDescription: ") + args.description;
        ss << LLUTILS_TEXT(
            "\n==================================================================================================\n");
        ss << introMessage << std::endl;

        if (args.systemErrorMessage.empty() == false)
            ss << LLUTILS_TEXT("System error: ") << args.systemErrorMessage;

        ss << LLUTILS_TEXT("call stack:") << std::endl;

        if (seperatedCallStack.empty() == true)
            ss << LLUtils::Exception::FormatStackTrace(
                args.stackTrace, args.exceptionmode == LLUtils::Exception::Mode::Error ? 3 : 0xFFF);
        else
            ss << seperatedCallStack;

        mLogFile.Log(ss.str());
    }

    bool ViewerApplication::HandleEventCallback(const std::function<bool()>& callback) noexcept
    {
        try
        {
            return callback();
        }
        catch (const LLUtils::Exception&)
        {
            return true;
        }
        catch (const std::exception& exception)
        {
            LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::RuntimeError, exception.what());
            return true;
        }
        catch (...)
        {
            LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::Unknown, "Unhandled native callback exception");
            return true;
        }
    }

    std::function<void()> ViewerApplication::MakeSafeCallback(std::function<void()> callback)
    {
        return [this, callback = std::move(callback)]()
        {
            HandleEventCallback(
                [&]()
                {
                    callback();
                    return true;
                });
        };
    }

    ViewerApplication::ViewerApplication(LWS::PlatformContext& platform)
        : fPlatform(platform), fRefreshTimer(platform,
                                             [this]()
                                             {
                                                 HandleEventCallback(
                                                     [this]()
                                                     {
                                                         OnRefreshTimer();
                                                         return true;
                                                     });
                                             }),
          fWindow(platform), fRefreshOperation(
                                 [this]()
                                 {
                                     HandleEventCallback(
                                         [this]()
                                         {
                                             OnRefresh();
                                             return true;
                                         });
                                 }),
          fPreserveImageSpaceSelection(
              [this]()
              {
                  HandleEventCallback(
                      [this]()
                      {
                          OnPreserveSelectionRect();
                          return true;
                      });
              }),
          fSelectionRect(
              [this](const LLUtils::RectI32& rect, bool visible)
              {
                  HandleEventCallback(
                      [&]()
                      {
                          OnSelectionRectChanged(rect, visible);
                          return true;
                      });
              }),
          fVirtualStatusBar(&fLabelManager,
                            [this]()
                            {
                                HandleEventCallback(
                                    [this]()
                                    {
                                        OnLabelRefreshRequest();
                                        return true;
                                    });
                            }),
          fTimerTopMostRetention(platform), fTimerSlideShow(platform), fClipboardHelper(platform),
          fTimerNoActiveZoom(platform), fTimerNavigation(platform),
          fFreeType(std::make_unique<FreeType::FreeTypeConnector>()), fLabelManager(fFreeType.get()),
          fImageOpenController(
              std::make_unique<ImageOpenController>(std::make_unique<OIVImageFileLoader>(fImageLoader))),
          fContextMenuTimer(platform), fSequencerTimer(platform)

    //, fFileCache(&fImageLoader, std::bind(&ViewerApplication::OnImageReady, this, std::placeholders::_1))

    {
        InitializePlatformState();
        fMouseInput = std::make_unique<ViewerMouseInput>(*this);
        fCommandController.SetResultSink([this](const LLUtils::native_string_type& message)
                                         { SetUserMessage(message); });

        // LLUtils::Exception::SetThrowErrorsInDebug(false);
        EventManager::GetSingleton().MonitorChange.Add(
            std::bind(&ViewerApplication::OnMonitorChanged, this, std::placeholders::_1));

        // OIV library exception forwarding is disabled because LLUtils::Exception::OnException is global.
        // Registering this callback logs the same crash once through the library bridge and once through the viewer.
        // OIV_CMD_RegisterCallbacks_Request request;
        //
        // request.OnException = [](OIV_Exception_Args args, void* userPointer)
        // {
        //     using namespace std;
        //     // Convert from C to C++
        //     LLUtils::Exception::EventArgs localArgs;
        //     localArgs.errorCode = static_cast<LLUtils::Exception::ErrorCode>(args.errorCode);
        //     localArgs.functionName = args.functionName;
        //
        //     localArgs.description = args.description;
        //     localArgs.systemErrorMessage = args.systemErrorMessage;
        //     reinterpret_cast<ViewerApplication*>(userPointer)->HandleException(true, localArgs, args.callstack);
        // };
        // request.userPointer = this;
        //
        // fRenderGateway->RegisterCallbacks(request);

        fExceptionConnection = LLUtils::Exception::OnException.Connect([this](LLUtils::Exception::EventArgs args)
                                                                       { HandleException(false, args, {}); });
    }

    void ViewerApplication::OnLabelRefreshRequest()
    {
        fRefreshOperation.Queue();
    }

    void ViewerApplication::OnMonitorChanged(const EventManager::MonitorChangeEventParams& params)
    {
        fCurrentMonitorProperties = params.monitorDesc;

        // update the refresh rate.
        fRefreshRateTimes1000 = params.monitorDesc.displayFrequency == 59 ? 59940
                                                                          : params.monitorDesc.displayFrequency * 1000;

        // DPI adjustment. The mouse generates movement events as district units.
        // To keep movement speed constant across several monitors in terms of distance,
        // DPI must be taken care into consideration.
        fDPIadjustmentFactor = {params.monitorDesc.contentScale.x, params.monitorDesc.contentScale.y};
    }

    void ViewerApplication::PerformRefresh()
    {
        using namespace std::chrono;

        const high_resolution_clock::time_point now = high_resolution_clock::now();
        const auto decision = FrameLimiterPolicy::Decide(EnableFrameLimiter, fRefreshTimer.GetEnabled(),
                                                         duration_cast<microseconds>(now - fLastRefreshTime).count(),
                                                         fRefreshRateTimes1000);

        switch (decision.action)
        {
            case FrameRefreshAction::RefreshNow:
                if (EnableFrameLimiter == true)
                    fRefreshTimer.Enable(false);

                fRenderGateway->Refresh();
                fLastRefreshTime = now;
                break;
            case FrameRefreshAction::ScheduleRefresh:
                fRefreshTimer.SetDueTime(static_cast<uint32_t>(decision.delayMs));
                fRefreshTimer.Enable(true);
                break;
            case FrameRefreshAction::None:
                break;
        }
    }

    void ViewerApplication::OnRefresh()
    {
        PerformRefresh();
    }

    // callback from a too early refresh operation

    void ViewerApplication::OnRefreshTimer()
    {
        using namespace std::chrono;
        fRenderGateway->Refresh();
        fLastRefreshTime = high_resolution_clock::now();
    }

    void ViewerApplication::OnPreserveSelectionRect()
    {
        LoadImageSpaceSelection();
    }

    LWS::Handle ViewerApplication::GetWindowHandle() const
    {
#ifdef LWS_HAS_WIN32_BACKEND
        return reinterpret_cast<LWS::Handle>(*LWS::Win32::GetHwnd(fWindow.GetWindow()));
#else
        return reinterpret_cast<LWS::Handle>(*LWS::Wayland::GetSurface(fWindow.GetWindow()));
#endif
    }

    void ViewerApplication::UpdateTitle()
    {
        const static LLUtils::native_string_type cachedVersionString = []
        {
#if OIV_OFFICIAL_RELEASE == 1
            auto title = LLUtils::native_string_type(LLUTILS_TEXT("OpenImageViewer ")) +
                         LLUtils::StringUtility::ToNativeString(FormatReleaseVersion(CurrentVersion));
    #ifdef OIV_RELEASE_SUFFIX
            title += OIV_RELEASE_SUFFIX;
    #endif
#else
            auto title = LLUtils::native_string_type(LLUTILS_TEXT("OpenImageViewer ")) +
                         LLUtils::StringUtility::ToNativeString(FormatFullVersion(CurrentVersion)) + LLUTILS_TEXT("-") +
                         LLUtils::StringUtility::ToNativeString(std::string(OIV_GIT_SHORT_HASH));

    #if LLUTILS_ARCH_TYPE == LLUTILS_ARCHITECTURE_64
            title += LLUTILS_TEXT(" | 64 bit");
    #else
            title += LLUTILS_TEXT(" | 32 bit");
    #endif
            title += LLUTILS_TEXT(" | ") + MessageHelper::GetFileTime(GetApplicationModulePath());
#endif

#if OIV_OFFICIAL_BUILD == 0
            title += LLUTILS_TEXT(" | UNOFFICIAL");
#endif
            return title;
        }();

        LLUtils::native_string_type title;
        if (fImageState.GetOpenedImage() != nullptr)
        {
            const ImageSource imageSource = fImageState.GetOpenedImage()->GetImageSource();
            if (imageSource == ImageSource::File)
            {
                const auto& committedCurrentFile = fBrowseSessionController != nullptr
                                                       ? fBrowseSessionController->GetCommittedCurrentFile()
                                                       : LLUtils::native_string_type{};
                auto decomposedPath              = MessageFormatter::DecomposePath(
                    committedCurrentFile.empty() ? GetOpenedFileName() : committedCurrentFile);
                bool includeIndex   = false;
                size_t displayIndex = 0;
                size_t fileCount    = 0;
                if (GetAppActive() == true && fBrowseSessionController != nullptr)
                {
                    const auto& fileList = fBrowseSessionController->GetFolderFileList();
                    if (fileList.IsIndexValid(fileList.GetCurrentIndex()))
                    {
                        includeIndex = true;
                        displayIndex = fileList.GetCurrentIndex() + 1;
                        fileCount    = fileList.GetSize();
                    }
                }

                title = ViewerPresentationPolicy::FormatFileTitlePrefix(decomposedPath.fileName,
                                                                        decomposedPath.extension,
                                                                        decomposedPath.parentPath, includeIndex,
                                                                        displayIndex, fileCount);
            }
            else
                title = ViewerPresentationPolicy::FormatNonFileTitlePrefix(imageSource);
        }
        std::ignore = fWindow.GetWindow().SetTitle(ViewerPresentationPolicy::FormatTitle(title, cachedVersionString));
    }

    void ViewerApplication::OnContextMenuTimer()
    {
        fContextMenuTimer.SetInterval(0);
        const auto pos  = fPlatform.GetMousePosition().value_or(LWS::Point{});
        auto chosenItem = fContextMenu->Show(pos.x - 16, pos.y + -16, AlignmentHorizontal::Center,
                                             AlignmentVertical::Center);
        fMouseInput->Cancel();

        if (chosenItem != nullptr)
        {
            CommandRequestIntenal request;
            request.commandName = chosenItem->userData.command;
            request.args        = chosenItem->userData.args;
            ExecuteCommandInternal(request);
        }
    }

    void ViewerApplication::OnSettingChange(const LLUtils::native_string_type& key,
                                            const LLUtils::native_string_type& value)
    {
        const AppSettingsPolicy::Action action = AppSettingsPolicy::ParseAction(key, value);

        switch (action.type)
        {
            case AppSettingsPolicy::ActionType::MaxZoom:
                fMaxPixelSize = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::ImageMarginX:
                fImageMargins.x = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::ImageMarginY:
                fImageMargins.y = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::MinImageSize:
                fMinImageSize = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::SlideshowInterval:
                fSlideshowPolicy.SetIntervalMs(static_cast<uint32_t>(action.integralValue));
                if (fSlideshowPolicy.IsEnabled())
                    fTimerSlideShow.SetInterval(fSlideshowPolicy.GetTimerIntervalMs());
                break;
            case AppSettingsPolicy::ActionType::QuickBrowseDelay:
                fQuickBrowseDelay = static_cast<uint16_t>(action.integralValue);
                break;
            case AppSettingsPolicy::ActionType::AutoScrollDeadZoneRadius:
                fAutoScroll->SetDeadZoneRadius(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorIn:
                fAutoScroll->SetSpeedFactorIn(action.floatValue);
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorOut:
                fAutoScroll->SetSpeedFactorOut(action.floatValue);
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorRange:
                fAutoScroll->SetSpeedFactorRange(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::AutoScrollMaxSpeed:
                fAutoScroll->SetMaxSpeed(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::DeletedFileRemovalMode:
                fDeletedFileRemovalMode = action.deletedFileRemovalMode;
                break;
            case AppSettingsPolicy::ActionType::FileReloadMode:
                fFileReloadPolicy.SetMode(action.fileReloadMode);
                break;
            case AppSettingsPolicy::ActionType::ReloadSettingsFileIfChanged:
                fReloadSettingsFileIfChanged = action.boolValue;
                break;
            case AppSettingsPolicy::ActionType::DefaultSortMode:
                fFileSorter.SetSortType(action.sortType);
                break;
            case AppSettingsPolicy::ActionType::SortDirection:
                fFileSorter.SetSortDirection(action.sortType, action.sortDirection);
                break;
            case AppSettingsPolicy::ActionType::BackgroundColor:
                ApiGlobal::sPictureRenderer->SetBackgroundColor(
                    action.backgroundColorIndex,
                    LLUtils::Color::FromString(LLUtils::StringUtility::ToAString(action.textValue)));
                fRefreshOperation.Queue();
                break;
            case AppSettingsPolicy::ActionType::BiggestSubImageOnLoad:
                fDisplayBiggestSubImageOnLoad = action.boolValue;
                break;
            case AppSettingsPolicy::ActionType::None:
                break;
        }
    }

    void ViewerApplication::LoadSettings()
    {
        auto settings = ConfigurationLoader::LoadSettings();
        for (const auto& pair : settings)
            OnSettingChange(LLUtils::StringUtility::ToNativeString(pair.first),
                            LLUtils::StringUtility::ToNativeString(pair.second));
    }

    void ViewerApplication::OnNotificationIcon(LWS::NotificationIconGroup::NotificationIconEventArgs args)
    {
        switch (args.action)
        {
            case LWS::NotificationIconGroup::NotificationIconAction::Select:
                if (!fWindow.GetWindow().GetVisible() ||
                    fWindow.GetWindow().GetShowState() == LWS::WindowShowState::Minimized)
                {
                    std::ignore = fWindow.GetWindow().SetVisible(true);
                    std::ignore = fWindow.GetWindow().RequestShowState(LWS::WindowShowState::Restored);
                    std::ignore = fWindow.GetWindow().RequestActivation();
                }
                else
                {
                    std::ignore = fWindow.GetWindow().SetVisible(false);
                }
                break;
            case LWS::NotificationIconGroup::NotificationIconAction::ContextMenu:
            {
                auto rect       = GetNotificationIconRect(fNotificationIconID);
                auto bottomLeft = ShellIntegrationHelper::TrayContextMenuPosition(rect);

                std::ignore     = fWindow.GetWindow().RequestActivation();
                auto chosenItem = fNotificationContextMenu->Show(bottomLeft.x, bottomLeft.y, AlignmentHorizontal::Right,
                                                                 AlignmentVertical::Bottom);
                if (chosenItem != nullptr)
                {
                    CommandRequestIntenal request;
                    request.commandName = "cmd_view_state";
                    request.args = ShellIntegrationHelper::ViewCommandArgsFromTrayItem(chosenItem->itemDisplayName);
                    ExecuteCommandInternal(request);
                }
            }
            break;
            case LWS::NotificationIconGroup::NotificationIconAction::None:
                LL_EXCEPTION_UNEXPECTED_VALUE;
                break;
        }
    }

    void ViewerApplication::SetUserMessage(const LLUtils::native_string_type& message, GroupID groupID,
                                           MessageFlags groupFlags)
    {
        fMessageManager->SetUserMessage(groupID, groupFlags, message);
    }

    void ViewerApplication::ShowImageInfo()
    {
        if (IsImageOpen())
        {
            CountColorsAsync();

            LLUtils::native_string_type imageInfoString = MessageHelper::CreateImageInfoMessage(
                fImageState.GetOpenedImage(), fImageState.GetImage(ImageChainStage::SourceImage),
                fImageLoader.GetImageCodec());
            OIVTextImage* imageInfoText = fLabelManager.GetOrCreateTextLabel("imageInfo");

            imageInfoText->SetText(imageInfoString);
            imageInfoText->SetBackgroundColor(LLUtils::Color(0, 0, 0, 127));
            imageInfoText->SetFontPath(LabelManager::sFixedFontPath);
            imageInfoText->SetFontSize(12);
            // imageInfoText->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
            imageInfoText->SetOutlineWidth(2);
            imageInfoText->SetPosition({20, 60});

            if (imageInfoText->IsDirty())
                fRefreshOperation.Queue();
        }
    }

    void ViewerApplication::ShowWelcomeMessage()
    {
        using namespace std;

        string message = "<textcolor=#4a80e2>Welcome to <textcolor=#dd0f1d>OIV\n"
                         "<textcolor=#25bc25>Drag <textcolor=#4a80e2>here an image to start\n"
                         "Press <textcolor=#25bc25>F1<textcolor=#4a80e2> to show key bindings";

        OIVTextImage* welcomeMessage = fLabelManager.GetOrCreateTextLabel("welcomeMessage");

        LLUtils::native_string_type wmsg;
        wmsg += LLUtils::StringUtility::ToNativeString(message);

        welcomeMessage->SetText(wmsg);
        welcomeMessage->SetBackgroundColor(LLUtils::Color(0));
        welcomeMessage->SetFontPath(LabelManager::sFontPath);
        welcomeMessage->SetFontSize(44);
        welcomeMessage->SetOutlineWidth(3);

        welcomeMessage->Create();
        // get the text size to reposition on screen
        using namespace LLUtils;
        const LWS::PixelSize canvasSize = fWindow.GetCanvasPixelSize();
        PointI32 clientSize{canvasSize.x, canvasSize.y};
        PointI32 center = (clientSize - static_cast<PointI32>(welcomeMessage->GetImage()->GetDimensions())) / 2;
        welcomeMessage->SetPosition(static_cast<PointF64>(center));

        if (welcomeMessage->IsDirty())
            fRefreshOperation.Queue();
    }

    void ViewerApplication::UnloadWelcomeMessage()
    {
        fLabelManager.Remove("welcomeMessage");
    }
}  // namespace OIV
