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

    using netsettings_Create_func = void (*)(GuiCreateParams*);
    using netsettings_SetVisible_func = void (*)(bool);
    using netsettings_SaveSettings_func = void (*)();

    struct SettingsContext
    {
        bool created;
        netsettings_Create_func Create;
        netsettings_SetVisible_func SetVisible;
        netsettings_SaveSettings_func SaveSettings;
    };
    SettingsContext settingsContext{};

    void ViewerApplication::NetSettingsCallback(ItemChangedArgs* args)
    {
        OnSettingChange(args->key, args->val);
    }

    void ViewerApplication::ShowSettings()
    {
        if (settingsContext.created == false)
        {
            using namespace std::filesystem;
            const path programPath = path(LLUtils::PlatformUtility::GetExeFolder());
            const path netsettingsPath = programPath / "Extensions" / "NetSettings";
            const path cliAdapterPath = netsettingsPath / "CliAdapter.dll";

            if (exists(cliAdapterPath))
            {
                SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                [[maybe_unused]] auto directory = AddDllDirectory(
                    (netsettingsPath.lexically_normal().wstring() + L"\\").c_str());
                HMODULE dllModule = LoadLibrary(cliAdapterPath.c_str());
                if (dllModule != nullptr)
                {
                    settingsContext.Create = reinterpret_cast<netsettings_Create_func>(
                        GetProcAddress(dllModule, "netsettings_Create"));
                    settingsContext.SetVisible = reinterpret_cast<netsettings_SetVisible_func>(
                        GetProcAddress(dllModule, "netsettings_SetVisible"));
                    settingsContext.SaveSettings = reinterpret_cast<netsettings_SaveSettings_func>(
                        GetProcAddress(dllModule, "netsettings_SaveUserSettings"));

                    GuiCreateParams params{};
                    params.userData = this;
                    params.callback = &::OIV::ViewerApplication::NetSettingsCallback_;
                    auto templateFile = (netsettingsPath / "Resources/GuiTemplate.json");
                    params.templateFilePath = templateFile.c_str();
                    auto userSettingsFile = (programPath / "Resources/Configuration/Settings.json");
                    params.userSettingsFilePath = userSettingsFile.c_str();

                    settingsContext.Create(&params);
                    settingsContext.SetVisible(true);
                    settingsContext.created = true;
                }
                else
                {
                    LLUtils::Logger::GetSingleton().Log(std::wstring(L"Cannot load Netsettings extension, error: ") +
                                                        LLUtils::PlatformUtility::GetLastErrorAsString<wchar_t>());
                }
            }
            else
            {
                LLUtils::Logger::GetSingleton().Log(std::wstring(L"Cannot load Netsettings extension, not found"));
            }
        }
        else
        {
            settingsContext.SetVisible(true);
        }
    }

    std::wstring ViewerApplication::GetLogFilePath()
    {
        auto GetVersionAsString = []
        {
            constexpr auto dot = OIV_TEXT(".");
            OIVStringStream ss;
            ss << OIV_VERSION_MAJOR << dot << OIV_VERSION_MINOR << dot << OIV_VERSION_BUILD << dot
               << OIV_VERSION_REVISION;
            return ss.str();
        };

        return GetAppDataFolder() + GetVersionAsString() + L"/oiv.log";
    }

    void ViewerApplication::HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args,
                                  std::wstring seperatedCallStack)
    {
        using namespace std;
        wstringstream ss;
        std::wstring source = isFromLibrary ? L"OIV library" : L"OIV viewer";
        const wstring introMessage = LLUtils::Exception::ExceptionErrorCodeToString(args.errorCode) +
                                     L" exception has occured at " + args.functionName + L" at " + source +
                                     L".\nDescription: " + args.description;
        const wstring displayMessage = introMessage + L"\nPlease refer to the log file [" + mLogFile.GetLogPath() +
                                       L"] for more information";

        ss << L"\n==================================================================================================\n";
        ss << introMessage << endl;

        if (args.systemErrorMessage.empty() == false)
            ss << "System error: " << args.systemErrorMessage;

        ss << "call stack:" << endl;

        if (seperatedCallStack.empty() == true)
            ss << LLUtils::Exception::FormatStackTrace(
                args.stackTrace, args.exceptionmode == LLUtils::Exception::Mode::Error ? 3 : 0xFFF);
        else
            ss << seperatedCallStack;

        mLogFile.Log(ss.str());
        // if (args.exceptionmode == LLUtils::Exception::Mode::Error)
        //   MessageBoxW(IsMainThread() ? fWindow.GetHandle() : nullptr, displayMessage.c_str(), L"Unhandled exception
        //   has occured.", MB_OK | MB_APPLMODAL);
        // DebugBreak();
    }

    ViewerApplication::~ViewerApplication()
    {
        fIsShuttingDown = true;

        if (fCountingColorsThread.joinable())
            fCountingColorsThread.join();

        RemoveExceptionHandler();
    }

    ViewerApplication::ViewerApplication()
        : fRefreshTimer(std::bind(&ViewerApplication::OnRefreshTimer, this)),
          fRefreshOperation(std::bind(&ViewerApplication::OnRefresh, this)),
          fPreserveImageSpaceSelection(std::bind(&ViewerApplication::OnPreserveSelectionRect, this)),
          fSelectionRect(
              std::bind(&ViewerApplication::OnSelectionRectChanged, this, std::placeholders::_1, std::placeholders::_2)),
          fVirtualStatusBar(&fLabelManager, std::bind(&ViewerApplication::OnLabelRefreshRequest, this)),
          fFreeType(std::make_unique<FreeType::FreeTypeConnector>()), fLabelManager(fFreeType.get()),
          fImageLoadController(std::make_unique<ImageLoadController>(std::make_unique<OIVImageFileLoader>(fImageLoader))),
          fEventSync(std::bind(&ViewerApplication::OnMessageFromBackgroundThread, this, std::placeholders::_1))

    //, fFileCache(&fImageLoader, std::bind(&ViewerApplication::OnImageReady, this, std::placeholders::_1))

    {
        fCommandController.SetResultSink([this](const std::wstring& message) { SetUserMessage(message); });

        // LLUtils::Exception::SetThrowErrorsInDebug(false);
        EventManager::GetSingleton().MonitorChange.Add(
            std::bind(&ViewerApplication::OnMonitorChanged, this, std::placeholders::_1));

        RegisterExceptionhandler();

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
        // fRenderGateway.RegisterCallbacks(request);

        LLUtils::Exception::OnException.Add([this](LLUtils::Exception::EventArgs args)
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
        fRefreshRateTimes1000 = params.monitorDesc.DisplaySettings.dmDisplayFrequency == 59
                                    ? 59940
                                    : params.monitorDesc.DisplaySettings.dmDisplayFrequency * 1000;

        const LLUtils::PointF64 BaseDPI{96.0, 96.0};

        // DPI adjustment. The mouse generates movement events as district units.
        // To keep movement speed constant across several monitors in terms of distance,
        // DPI must be taken care into consideration.
        fDPIadjustmentFactor = LLUtils::PointF64{static_cast<LLUtils::PointF64::point_type>(params.monitorDesc.DPIx),
                                                 static_cast<LLUtils::PointF64::point_type>(params.monitorDesc.DPIy)} /
                               BaseDPI;
    }

    void ViewerApplication::ProbeForMonitorChange()
    {
        if (fIsFirstFrameDisplayed == true)
            fMonitorProvider.UpdateFromWindowHandle(fWindow.GetHandle());
    }

    void ViewerApplication::PerformRefresh()
    {
        using namespace std::chrono;

        if (EnableFrameLimiter == true)
        {
            ProbeForMonitorChange();
        }

        const high_resolution_clock::time_point now = high_resolution_clock::now();
        const auto decision = FrameLimiterPolicy::Decide(
            EnableFrameLimiter,
            fRefreshTimer.GetEnabled(),
            duration_cast<microseconds>(now - fLastRefreshTime).count(),
            fRefreshRateTimes1000);

        switch (decision.action)
        {
            case FrameRefreshAction::RefreshNow:
                if (EnableFrameLimiter == true)
                    fRefreshTimer.Enable(false);

                fRenderGateway.Refresh();
                fLastRefreshTime = now;
                break;
            case FrameRefreshAction::ScheduleRefresh:
                fRefreshTimer.SetDueTime(static_cast<DWORD>(decision.delayMs));
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
        fRenderGateway.Refresh();
        fLastRefreshTime = high_resolution_clock::now();
    }

    void ViewerApplication::OnPreserveSelectionRect()
    {
        LoadImageSpaceSelection();
    }

    HWND ViewerApplication::GetWindowHandle() const
    {
        return fWindow.GetHandle();
    }

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

    void ViewerApplication::UpdateTitle()
    {
        const static LLUtils::native_string_type cachedVersionString =
            OIV_TEXT("OpenImageViewer ") + std::to_wstring(OIV_VERSION_MAJOR) + L'.' +
            std::to_wstring(OIV_VERSION_MINOR) +
            (OIV_VERSION_REVISION != 0
                 ? (std::wstring(L".") + LLUtils::StringUtility::ToNativeString(OIV_VERSION_REVISION))
                 : std::wstring{})

        // If not official release add revision and build number
#if OIV_OFFICIAL_RELEASE == 0

            + L"." + WIDEN(GIT_HASH_ID) + L"." + std::to_wstring(OIV_VERSION_BUILD)
    #if LLUTILS_ARCH_TYPE == LLUTILS_ARCHITECTURE_64
            + OIV_TEXT(" | 64 bit")
    #else
            + OIV_TEXT(" | 32 bit")
    #endif
            + OIV_TEXT(" | ") + MessageHelper::GetFileTime(LLUtils::PlatformUtility::GetDllPath())
#else
    #ifdef OIV_RELEASE_SUFFIX
            + OIV_RELEASE_SUFFIX
    #endif
#endif

        // If not official build, i.e. from unofficial / unknown source, add an "UNOFFICIAL" remark.
#if OIV_OFFICIAL_BUILD == 0

            + OIV_TEXT(" | UNOFFICIAL")
#endif
            ;
        std::wstring title;
        if (fImageState.GetOpenedImage() != nullptr)
        {
            const ImageSource imageSource = fImageState.GetOpenedImage()->GetImageSource();
            if (imageSource == ImageSource::File)
            {
                auto decomposedPath = MessageFormatter::DecomposePath(GetOpenedFileName());
                bool includeIndex = false;
                size_t displayIndex = 0;
                size_t fileCount = 0;
                if (GetAppActive() == true && fFileSessionController != nullptr)
                {
                    const auto& fileList = fFileSessionController->GetFileList();
                    includeIndex = true;
                    displayIndex = fileList.GetCurrentIndex() == FileList::IndexStart ? 0 : fileList.GetCurrentIndex() + 1;
                    fileCount = fileList.GetSize();
                }

                title = ViewerPresentationPolicy::FormatFileTitlePrefix(decomposedPath.fileName,
                                                                         decomposedPath.extension,
                                                                         decomposedPath.parentPath,
                                                                         includeIndex,
                                                                         displayIndex,
                                                                         fileCount);
            }
            else
                title = ViewerPresentationPolicy::FormatNonFileTitlePrefix(imageSource);
        }
        fWindow.SetTitle(ViewerPresentationPolicy::FormatTitle(title, cachedVersionString));
    }

    void ViewerApplication::OnContextMenuTimer()
    {
        fContextMenuTimer.SetInterval(0);
        auto pos = ::Win32::Win32Helper::GetMouseCursorPosition();
        auto chosenItem = fContextMenu->Show(pos.x - 16, pos.y + -16, AlignmentHorizontal::Center,
                                             AlignmentVertical::Center);

        if (chosenItem != nullptr)
        {
            CommandRequestIntenal request;
            request.commandName = chosenItem->userData.command;
            request.args = chosenItem->userData.args;
            ExecuteCommandInternal(request);
        }
    }

    void ViewerApplication::OnSettingChange(const std::wstring& key, const std::wstring& value)
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
            OnSettingChange(LLUtils::StringUtility::ToWString(pair.first),
                            LLUtils::StringUtility::ToWString(pair.second));
    }

    void ViewerApplication::OnNotificationIcon(::Win32::NotificationIconGroup::NotificationIconEventArgs args)
    {
        using namespace ::Win32;
        switch (args.action)
        {
            case NotificationIconGroup::NotificationIconAction::Select:
                if (fWindow.GetVisible() == false)
                {
                    fWindow.SetVisible(true);
                    fWindow.SetForground();
                }
                else
                {
                    fWindow.SetVisible(false);
                }
                break;
            case NotificationIconGroup::NotificationIconAction::ContextMenu:
            {
                auto rect = fNotificationIcons.GetIconRect(fNotificationIconID);
                auto bottomLeft = ShellIntegrationHelper::TrayContextMenuPosition(rect);

                fWindow.SetForground();
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
            case NotificationIconGroup::NotificationIconAction::None:
                LL_EXCEPTION_UNEXPECTED_VALUE;
                break;
        }
    }

    void ViewerApplication::SetUserMessage(const std::wstring& message, GroupID groupID, MessageFlags groupFlags)
    {
        fMessageManager->SetUserMessage(groupID, groupFlags, message);
    }

    void ViewerApplication::ShowImageInfo()
    {
        if (IsImageOpen())
        {
            CountColorsAsync();

            std::wstring imageInfoString = MessageHelper::CreateImageInfoMessage(
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

        std::wstring wmsg;
        wmsg += LLUtils::StringUtility::ToWString(message);

        welcomeMessage->SetText(wmsg);
        welcomeMessage->SetBackgroundColor(LLUtils::Color(0));
        welcomeMessage->SetFontPath(LabelManager::sFontPath);
        welcomeMessage->SetFontSize(44);
        welcomeMessage->SetOutlineWidth(3);

        welcomeMessage->Create();
        // get the text size to reposition on screen
        using namespace LLUtils;
        PointI32 clientSize = fWindow.GetCanvasSize();
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
