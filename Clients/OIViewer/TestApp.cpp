#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "TestApp.h"

#include <Windows.h>
#include <Version.h>

#include <functions.h>
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

#include "win32/UserMessages.h"
#include "OIVCommands.h"
#include "SelectionRect.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "Helpers/OIVImageHelper.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "globals.h"
#include "ConfigurationLoader.h"
#include "CommandRegistry.h"
#include "Helpers/PixelHelper.h"
#include "ExceptionHandler.h"
#include <oivappcore/ColorCountPolicy.h>
#include <oivappcore/ColorCorrectionCommandPolicy.h>
#include <oivappcore/FileChangePolicy.h>
#include <oivappcore/FrameLimiterPolicy.h>
#include <oivappcore/ImageEditPolicy.h>
#include <oivappcore/ImageFormatCatalogPolicy.h>
#include <oivappcore/ImageLoadPresentationPolicy.h>
#include <oivappcore/ImageTransformCommandPolicy.h>
#include <oivappcore/InputGesturePolicy.h>
#include <oivappcore/SelectionWorkflowPolicy.h>
#include <oivappcore/SequencerPolicy.h>
#include <oivappcore/SortCommandPolicy.h>
#include <oivappcore/SubImagePolicy.h>
#include <oivappcore/ViewActionController.h>
#include <oivappcore/ViewCommandPolicy.h>
#include <oivappcore/ViewerPresentationPolicy.h>
#include <ImageUtil/ImageUtil.h>
#include "InterThreadMessages.h"

#include "resource.h"

namespace OIV
{
    namespace
    {
        struct FileIndexResidencyReadyData
        {
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };

        struct FolderLoadResidencyReadyData
        {
            BrowseResidencyManager::FileListSnapshot snapshot;
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };
    }  // namespace

    void TestApp::CMD_Zoom(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        if (IsImageOpen())
        {
            const ZoomCommand command = ViewCommandPolicy::ParseZoom(request.args);
            ZoomInternal(command.amount, command.centerX, command.centerY);
            result.resValue = ViewCommandPolicy::FormatZoomResult(GetScale());
        }
    }

    void TestApp::CMD_ViewState(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        string type = request.args.GetArgValue("type");

        bool fullscreenModeChanged = false;
        bool filterTypeChanged = false;

        if (type == "toggleBorders")
        {
            ToggleBorders();
            result.resValue = std::wstring(L"Borders ") + (fShowBorders == true ? L"On" : L"Off");
        }
        else if (type == "quit")
        {
            string op = request.args.GetArgValue("op");
            bool closeToTray = op == "closetotray";
            CloseApplication(closeToTray);
        }
        else if (type == "grid")
        {
            ToggleGrid();
            result.resValue = L"Grid ";
            result.resValue += fIsGridEnabled == true ? L"on" : L"off";
        }
        else if (type == "slideShow")
        {
            SetSlideShowEnabled(!GetSlideShowEnabled());
            result.resValue = L"Slideshow ";
            result.resValue += fTimerSlideShow.GetInterval() > 0 ? L"on" : L"off";
        }
        else if (type == "toggleNormalization")
        {
            // Change normalization mode
            fImageState.SetUseRainbowNormalization(!fImageState.GetUseRainbowNormalization());
            RefreshImage();
            result.resValue = fImageState.GetUseRainbowNormalization() ? L"Rainbow normalization"
                                                                       : L"Grayscale normalization";
        }
        else if (type == "imageFilterUp")
        {
            if (fImageState.GetVisibleImage() != nullptr)
            {
                SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(GetFilterType()) + 1));
                filterTypeChanged = true;
            }
        }
        else if (type == "imageFilterDown")
        {
            if (fImageState.GetVisibleImage() != nullptr)
            {
                SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(GetFilterType()) - 1));
                filterTypeChanged = true;
            }
        }
        else if (type == "toggleFullScreen")  // Toggle full screen
        {
            ToggleFullScreen(false);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleMultiFullScreen")  // Toggle multi full screen
        {
            ToggleFullScreen(true);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleresetoffset")
        {
            fResetTransformationMode = static_cast<ResetTransformationMode>(
                (static_cast<int>(fResetTransformationMode) + 1) % static_cast<int>(ResetTransformationMode::Count));
            result.resValue = fResetTransformationMode == ResetTransformationMode::DoNothing
                                  ? L"Don't auto reset image state"
                                  : L"Auto reset image state";
        }
        else if (type == "toggletransparencymode")
        {
            fTransparencyMode = static_cast<OIV_PROP_TransparencyMode>(
                (fTransparencyMode + 1) % static_cast<int>(OIV_PROP_TransparencyMode::TM_Count));
            UpdateRenderViewParams();

            std::wstring userMessage = L"Transparency: ";
            std::wstring transparencyMode;

            switch (fTransparencyMode)
            {
                case OIV_PROP_TransparencyMode::TM_Light:
                    transparencyMode = L"Light";
                    break;
                case OIV_PROP_TransparencyMode::TM_Medium:
                    transparencyMode = L"Medium(default)";
                    break;
                case OIV_PROP_TransparencyMode::TM_Dark:
                    transparencyMode = L"Dark";
                    break;
                case OIV_PROP_TransparencyMode::TM_Darker:
                    transparencyMode = L"Darker";
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            result.resValue = userMessage + L"<textcolor=#7672ff>" + transparencyMode;
        }
        else if (type == "toggledownsamplingtechnique")
        {
            DownscalingTechnique technique = GetNextEnumValue(fDownScalingTechnique);

            std::wstring downscaleTechnique;

            switch (technique)
            {
                case DownscalingTechnique::None:
                    downscaleTechnique = L"No downsamling";
                    break;
                case DownscalingTechnique::HardwareMipmaps:
                    downscaleTechnique = L"Hardware mipmaps";
                    break;
                case DownscalingTechnique::Software:
                    downscaleTechnique = L"Box filter";
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }
            result.resValue = DefaultTextKeyColorTag + L"Downscaling technique: " + DefaultTextValueColorTag +
                              downscaleTechnique;

            SetDownScalingTechnique(technique);
        }
        else if (type == "toggleStatusBar")
        {
            fVirtualStatusBar.SetVisible(!fVirtualStatusBar.GetVisible());
            fRefreshOperation.Queue();
        }

        if (fullscreenModeChanged == true)
        {
            switch (fWindow.GetFullScreenState())
            {
                case ::Win32::FullSceenState::MultiScreen:
                    result.resValue = L"Multi full screen";
                    break;
                case ::Win32::FullSceenState::SingleScreen:
                    result.resValue = L"Full screen";
                    break;
                case ::Win32::FullSceenState::Windowed:
                    result.resValue = L"Windowed";
                    break;
                case ::Win32::FullSceenState::None:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                    break;
            }
        }

        if (filterTypeChanged == true)
        {
            switch (GetFilterType())
            {
                case FT_None:
                    result.resValue = L"No filtering";
                    break;
                case FT_Linear:
                    result.resValue = L"Linear filtering";
                    break;
                case FT_Lanczos3:
                    result.resValue = L"Lanczos3 filtering";
                    break;
                case FT_Count:
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                    break;
            }
        }
    }

    void TestApp::SetImageInfoVisible(bool visible)
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

    bool TestApp::GetImageInfoVisible() const
    {
        return fImageInfoVisible;
    }

    void TestApp::NetSettingsCallback_(ItemChangedArgs* args)
    {
        reinterpret_cast<TestApp*>(args->userData)->NetSettingsCallback(args);
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

    void TestApp::NetSettingsCallback(ItemChangedArgs* args)
    {
        OnSettingChange(args->key, args->val);
    }

    void TestApp::ShowSettings()
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
                    params.callback = &::OIV::TestApp::NetSettingsCallback_;
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

    void TestApp::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request,
                                        [[maybe_unused]] CommandManager::CommandResult& result)
    {
        auto type = request.args.GetArgValue("type");
        if (type == "imageinfo")  // Toggle image info
        {
            SetImageInfoVisible(!GetImageInfoVisible());
        }
        else if (type == "keybindings")  // Toggle keybindings
        {
            OIVTextImage* text = fLabelManager.GetTextLabel("keyBindings");
            if (text != nullptr)  //
            {
                fLabelManager.Remove("keyBindings");
                fRefreshOperation.Queue();
                return;
            }

            text = fLabelManager.GetOrCreateTextLabel("keyBindings");
            auto message = MessageHelper::CreateKeyBindingsMessage();

            text->SetText(message);
            text->SetBackgroundColor({0, 0, 0, 216});
            text->SetFontPath(LabelManager::sFixedFontPath);
            text->SetFontSize(12);
            text->SetOutlineWidth(2);
            text->SetPosition({20, 60});
            text->SetFilterType(OIV_Filter_type::FT_None);
            text->SetImageRenderMode(IRM_Overlay);
            text->SetScale({1.0, 1.0});
            text->SetOpacity(1.0);
            text->SetVisible(true);

            if (text->IsDirty())
                fRefreshOperation.Queue();
        }
        else if (type == "settings")  // Show settings
        {
            ShowSettings();
        }
    }

    void TestApp::CMD_OpenFile([[maybe_unused]] const CommandManager::CommandRequest& request,
                               [[maybe_unused]] CommandManager::CommandResult& response)
    {
        std::string cmd = request.args.GetArgValue("cmd");
        if (cmd == "savefile")
        {
            using namespace ::Win32;
            std::wstring saveFilePath;
            std::wstring defaultFileName;
            if (IsImageOpen())
            {
                auto openedImage = fImageState.GetOpenedImage();
                auto imageSource = openedImage->GetImageSource();
                switch (imageSource)
                {
                    case ImageSource::ClipboardText:
                        defaultFileName = L"text";
                        break;
                    case ImageSource::File:
                    {
                        std::filesystem::path p = std::dynamic_pointer_cast<OIVFileImage>(openedImage)->GetFileName();
                        defaultFileName = p.stem();
                    }
                    break;
                    case ImageSource::Clipboard:
                        defaultFileName = L"clipboard";
                        break;
                    default:
                        defaultFileName = L"image";
                        break;
                }

                auto result = FileDialog::Show(FileDialogType::SaveFile, fSaveComDlgFilters.GetFilters(),
                                               L"Save an image", fWindow.GetHandle(), L"*." + fDefaultSaveFileExtension,
                                               fDefaultSaveFileFormatIndex, defaultFileName, saveFilePath);

                if (result == FileDialogResult::Success)
                {
                    std::wstring extension = LLUtils::StringUtility::ToLower(
                        std::filesystem::path(saveFilePath).extension().wstring());
                    std::wstring_view sv(extension);

                    if (sv.empty() == false)
                        sv = sv.substr(1);

                    auto rasterized = fImageState.GetImage(ImageChainStage::Rasterized)->GetImage();

                    if (IMUtil::ImageUtil::HasAlphaChannelAndInUse(rasterized) == false)
                        rasterized = IMUtil::ImageUtil::Convert(
                            rasterized, IMCodec::TexelFormat::I_R8_G8_B8);  // Get rid of the Alpha channel

                    LLUtils::Buffer encodedBuffer;

                    fImageLoader.Encode(rasterized, sv.data(), encodedBuffer);
                    LLUtils::File::WriteAllBytes(saveFilePath, encodedBuffer.size(), encodedBuffer.data());
                }
            }
        }

        else
        {
            using namespace ::Win32;
            std::wstring openFilePath;
            auto result = FileDialog::Show(FileDialogType::OpenFile, fOpenComDlgFilters.GetFilters(), L"Open image",
                                           fWindow.GetHandle(), {}, 0, {}, openFilePath);

            if (result == FileDialogResult::Success)
                LoadFile(openFilePath, IMCodec::PluginTraverseMode::NoTraverse);
        }
    }

    void TestApp::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request,
                                           CommandManager::CommandResult& response)
    {
        const AxisAlignedTransformCommand command =
            ImageTransformCommandPolicy::ParseAxisAlignedTransform(request.args);

        if (command.HasTransform())
        {
            TransformImage(command.rotation, command.flip);
            response.resValue = ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                fImageState.GetAxisAlignedRotation(),
                fImageState.GetAxisAlignedFlip());
        }
    }

    void TestApp::CMD_ToggleColorCorrection([[maybe_unused]] const CommandManager::CommandRequest& request,
                                            CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        if (ToggleColorCorrection())
            result.resValue = L"Reset color correction to previous";
        else
            result.resValue = L"Reset color correction to default";
    }

    void TestApp::CMD_SetWindowSize(const CommandManager::CommandRequest& request,
                                    CommandManager::CommandResult& result)
    {
        const auto& workArea = fCurrentMonitorProperties.monitorInfo.rcWork;
        const WindowSizeDecision decision = ViewCommandPolicy::DecideWindowSize(
            request.args,
            fWindow.GetWindowSize(),
            fWindow.GetPosition(),
            {workArea.left, workArea.top, workArea.right, workArea.bottom});

        switch (decision.mode)
        {
            case WindowSizeMode::Fullscreen:
                fWindow.SetFullScreenState(::Win32::FullSceenState::SingleScreen);
                break;
            case WindowSizeMode::MultiFullscreen:
                fWindow.SetFullScreenState(::Win32::FullSceenState::MultiScreen);
                break;
            case WindowSizeMode::Windowed:
                if (fWindow.GetFullScreenState() != ::Win32::FullSceenState::Windowed)
                    fWindow.SetFullScreenState(::Win32::FullSceenState::Windowed);

                if (decision.position != fWindow.GetPosition())
                    fWindow.SetPosition(decision.position.x, decision.position.y);

                fWindow.SetSize(decision.size.x, decision.size.y);
                break;
            case WindowSizeMode::None:
                break;
        }

        result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
    }

    void TestApp::CMD_SortFiles(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        const SortCommandDecision decision = SortCommandPolicy::Decide(request.args, fFileSorter.GetSortType());

        if (decision.valid)
        {
            if (decision.reverseDirection)
                fFileSorter.SetActiveSortDirection(SortCommandPolicy::Reverse(fFileSorter.GetActiveSortDirection()));
            else
                fFileSorter.SetSortType(decision.sortType);
        }

        SortFileList();
        result.resValue = SortCommandPolicy::FormatSortResult(request.displayName, fFileSorter.GetActiveSortDirection());
    }

    void TestApp::CMD_Sequencer(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        auto openedImage = fImageState.GetOpenedImage();
        if (openedImage != nullptr &&
            openedImage->GetImage()->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame)
        {
            if (SequencerPolicy::IsChangeSpeedCommand(request.args))
            {
                fCurrentSequencerSpeed = SequencerPolicy::ApplySpeedChange(
                    fCurrentSequencerSpeed,
                    SequencerPolicy::ParseSpeedChangePercent(request.args));
                result.resValue = SequencerPolicy::FormatSpeed(fCurrentSequencerSpeed);
            }
        }
    }

    void TestApp::CMD_DeleteFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        string type = request.args.GetArgValue("type");

        if (type == "recyclebin")
            DeleteOpenedFile(false);
        else if (type == "permanently")
            DeleteOpenedFile(true);

        result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
    }

    void TestApp::CMD_ColorCorrection(const CommandManager::CommandRequest& request,
                                      CommandManager::CommandResult& result)
    {
        const ColorCorrectionCommand command = ColorCorrectionCommandPolicy::Parse(request.args);
        if (command.IsValid() == false)
            return;

        double* value = nullptr;
        switch (command.channel)
        {
            case ColorCorrectionChannel::Gamma:
                value = &fColorExposure.gamma;
                break;
            case ColorCorrectionChannel::Exposure:
                value = &fColorExposure.exposure;
                break;
            case ColorCorrectionChannel::Offset:
                value = &fColorExposure.offset;
                break;
            case ColorCorrectionChannel::Saturation:
                value = &fColorExposure.saturation;
                break;
            case ColorCorrectionChannel::Contrast:
                value = &fColorExposure.contrast;
                break;
            case ColorCorrectionChannel::None:
                break;
        }

        if (value == nullptr)
            return;

        *value = ColorCorrectionCommandPolicy::Apply(*value, command.operation, command.value);
        result.resValue = ColorCorrectionCommandPolicy::FormatResult(command, *value);
        UpdateExposure();
    }

    void TestApp::CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        const PanCommand command = ViewCommandPolicy::ParsePan(request.args);

        switch (command.direction)
        {
            case PanDirection::Up:
                Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(command.amount)));
                break;
            case PanDirection::Down:
                Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(-command.amount)));
                break;
            case PanDirection::Left:
                Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(command.amount), 0));
                break;
            case PanDirection::Right:
                Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(-command.amount), 0));
                break;
            case PanDirection::None:
                break;
        }

        result.resValue = ViewCommandPolicy::FormatPanResult(request.displayName, command.amount);
    }

    void TestApp::CMD_CopyToClipboard(const CommandManager::CommandRequest& request,
                                      CommandManager::CommandResult& result)
    {
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");

        if (cmd == "fileName")
        {
            if (IsOpenedImageIsAFile())
            {
                fClipboardHelper.SetClipboardText(GetOpenedFileName().c_str());
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
            }
        }
        else if (cmd == "selectedArea")
        {
            OperationResult res = CopyVisibleToClipBoard();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(L"Cannot copy to clipboard", res);
            else
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }
        else if (cmd == "cut")
        {
            OperationResult res = CutSelectedArea();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(L"Cannot cut selected area", res);
            else
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }
    }

    void TestApp::CMD_PasteFromClipboard([[maybe_unused]] const CommandManager::CommandRequest& request,
                                         CommandManager::CommandResult& result)
    {
        switch (PasteFromClipBoard())
        {
            case ClipboardDataType::None:
                result.resValue = L"Nothing usable in clipboard";
                break;
            case ClipboardDataType::Image:
                result.resValue = L"Paste image from clipboard";
                break;
            case ClipboardDataType::Text:
                result.resValue = L"Paste text from clipboard";
                break;
        }
    }

    void TestApp::CMD_ImageManipulation(const CommandManager::CommandRequest& request,
                                        CommandManager::CommandResult& result)
    {
        using namespace std;
        const string cmd = request.args.GetArgValue("cmd");
        if (cmd == "cropSelectedArea")
        {
            OperationResult res = CropVisibleImage();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(L"Cannot crop selected area", res);
            else
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }

        else if (cmd == "selectAll")
        {
            if (fImageState.GetOpenedImage() != nullptr)
            {
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
                using namespace LLUtils;
                RectI32 imageInScreenSpace = static_cast<LLUtils::RectI32>(
                    ImageToClient({{0.0, 0.0}, {GetImageSize(ImageSizeType::Transformed)}}));

                fRefreshOperation.Begin();
                fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, {0, 0});
                fSelectionRect.SetSelection(SelectionRect::Operation::BeginDrag,
                                            imageInScreenSpace.GetCorner(Corner::TopLeft));
                fSelectionRect.SetSelection(SelectionRect::Operation::Drag,
                                            imageInScreenSpace.GetCorner(Corner::BottomRight));
                fSelectionRect.SetSelection(SelectionRect::Operation::EndDrag,
                                            imageInScreenSpace.GetCorner(Corner::BottomRight));

                SetImageSpaceSelection(
                    LLUtils::RectI32{{0, 0}, LLUtils::PointI32{GetImageSize(ImageSizeType::Transformed)}});

                fRefreshOperation.End();
            }
            else
            {
                result.resValue = L"No image loaded";
            }
        }
    }

    void TestApp::CMD_Placement(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        switch (ViewCommandPolicy::ParsePlacement(request.args))
        {
            case PlacementAction::OriginalSize:
                SetOriginalSize();
                break;
            case PlacementAction::FitToScreen:
                FitToClientAreaAndCenter();
                break;
            case PlacementAction::Center:
                Center();
                break;
            case PlacementAction::None:
                break;
        }

        result.resValue = ViewCommandPolicy::FormatPlacementResult(request.displayName);
    }

    void TestApp::CMD_Navigate(const CommandManager::CommandRequest& request,
                               [[maybe_unused]] CommandManager::CommandResult& result)
    {
        const NavigationCommand command = ViewCommandPolicy::ParseNavigation(request.args);
        if (command.subImage)
        {
            auto& imageList = fWindow.GetImageControl().GetImageList();
            const auto numElements = imageList.GetNumberOfElements();
            if (numElements > 0)
            {
                imageList.SetSelected(static_cast<int>(ViewCommandPolicy::NextSubImageIndex(
                    imageList.GetSelected(),
                    command.amount,
                    numElements)));
            }
        }
        else
        {
            JumpFiles(command.amount);
        }
    }

    void TestApp::CMD_Shell(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        result.resValue = ShellCommandHandler::Execute(request, GetOpenedFileName(), fImageState.GetOpenedImage());
    }

    template <typename T>
    std::wstring IntToHex(T val)
    {
        std::wstringstream ss;
        ss << std::setfill(L'0') << std::setw(sizeof(T) * 2) << std::hex << val;

        return ss.str();
    }

    std::wstring TestApp::GetAppDataFolder()
    {
        return LLUtils::PlatformUtility::GetAppDataFolder() + L"/OIV/";
    }

    HWND TestApp::FindTrayBarWindow()
    {
        using namespace Win32;

        HWND nextChild = nullptr;

        do
        {
            nextChild = FindWindowEx(nullptr, nextChild, ::Win32::Win32Window::WindowClassName, nullptr);
        } while (nextChild != nullptr && MainWindow::GetIsTrayWindow(nextChild) == false);

        return nextChild;
    }

    std::wstring TestApp::GetLogFilePath()
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

    void TestApp::HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args,
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

    TestApp::~TestApp()
    {
        fIsShuttingDown = true;

        if (fCountingColorsThread.joinable())
            fCountingColorsThread.join();

        RemoveExceptionHandler();
    }

    TestApp::TestApp()
        : fRefreshTimer(std::bind(&TestApp::OnRefreshTimer, this)),
          fRefreshOperation(std::bind(&TestApp::OnRefresh, this)),
          fPreserveImageSpaceSelection(std::bind(&TestApp::OnPreserveSelectionRect, this)),
          fSelectionRect(
              std::bind(&TestApp::OnSelectionRectChanged, this, std::placeholders::_1, std::placeholders::_2)),
          fVirtualStatusBar(&fLabelManager, std::bind(&TestApp::OnLabelRefreshRequest, this)),
          fFreeType(std::make_unique<FreeType::FreeTypeConnector>()), fLabelManager(fFreeType.get()),
          fImageLoadController(std::make_unique<ImageLoadController>(std::make_unique<OIVImageFileLoader>(fImageLoader))),
          fEventSync(std::bind(&TestApp::OnMessageFromBackgroundThread, this, std::placeholders::_1))

    //, fFileCache(&fImageLoader, std::bind(&TestApp::OnImageReady, this, std::placeholders::_1))

    {
        // LLUtils::Exception::SetThrowErrorsInDebug(false);
        EventManager::GetSingleton().MonitorChange.Add(
            std::bind(&TestApp::OnMonitorChanged, this, std::placeholders::_1));

        RegisterExceptionhandler();

        OIV_CMD_RegisterCallbacks_Request request;

        request.OnException = [](OIV_Exception_Args args, void* userPointer)
        {
            using namespace std;
            // Convert from C to C++
            LLUtils::Exception::EventArgs localArgs;
            localArgs.errorCode = static_cast<LLUtils::Exception::ErrorCode>(args.errorCode);
            localArgs.functionName = args.functionName;

            localArgs.description = args.description;
            localArgs.systemErrorMessage = args.systemErrorMessage;
            reinterpret_cast<TestApp*>(userPointer)->HandleException(true, localArgs, args.callstack);
        };
        request.userPointer = this;

        OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &NullCommand);

        LLUtils::Exception::OnException.Add([this](LLUtils::Exception::EventArgs args)
                                            { HandleException(false, args, {}); });
    }

    void TestApp::AddCommandsAndKeyBindings()
    {
        using namespace std;
        using namespace placeholders;

        CommandRegistry::AddConfiguredCommandsAndKeyBindings(fCommandManager, fKeyBindings);
        CommandRegistry::AddCommandCallbacks(
            fCommandManager,
            {{"cmd_color_correction", std::bind(&TestApp::CMD_ColorCorrection, this, _1, _2)},
             {"cmd_view_state", std::bind(&TestApp::CMD_ViewState, this, _1, _2)},
             {"cmd_toggle_correction", std::bind(&TestApp::CMD_ToggleColorCorrection, this, _1, _2)},
             {"cmd_toggle_keybindings", std::bind(&TestApp::CMD_ToggleKeyBindings, this, _1, _2)},
             {"cmd_axis_aligned_transform", std::bind(&TestApp::CMD_AxisAlignedTransform, this, _1, _2)},
             {"cmd_open_file", std::bind(&TestApp::CMD_OpenFile, this, _1, _2)},
             {"cmd_zoom", std::bind(&TestApp::CMD_Zoom, this, _1, _2)},
             {"cmd_pan", std::bind(&TestApp::CMD_Pan, this, _1, _2)},
             {"cmd_placement", std::bind(&TestApp::CMD_Placement, this, _1, _2)},
             {"cmd_copyToClipboard", std::bind(&TestApp::CMD_CopyToClipboard, this, _1, _2)},
             {"cmd_pasteFromClipboard", std::bind(&TestApp::CMD_PasteFromClipboard, this, _1, _2)},
             {"cmd_imageManipulation", std::bind(&TestApp::CMD_ImageManipulation, this, _1, _2)},
             {"cmd_navigate", std::bind(&TestApp::CMD_Navigate, this, _1, _2)},
             {"cmd_shell", std::bind(&TestApp::CMD_Shell, this, _1, _2)},
             {"cmd_delete_file", std::bind(&TestApp::CMD_DeleteFile, this, _1, _2)},
             {"cmd_set_window_size", std::bind(&TestApp::CMD_SetWindowSize, this, _1, _2)},
             {"cmd_sort_files", std::bind(&TestApp::CMD_SortFiles, this, _1, _2)},
             {"cmd_sequencer", std::bind(&TestApp::CMD_Sequencer, this, _1, _2)}});
    }

    void TestApp::OnLabelRefreshRequest()
    {
        fRefreshOperation.Queue();
    }

    void TestApp::OnMonitorChanged(const EventManager::MonitorChangeEventParams& params)
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

    void TestApp::ProbeForMonitorChange()
    {
        if (fIsFirstFrameDisplayed == true)
            fMonitorProvider.UpdateFromWindowHandle(fWindow.GetHandle());
    }

    void TestApp::PerformRefresh()
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

                OIVCommands::Refresh();
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

    void TestApp::OnSelectionRectChanged(const LLUtils::RectI32& selectionRect, bool isVisible)
    {
        if (isVisible)
            OIVCommands::SetSelectionRect(selectionRect);
        else
            OIVCommands::CancelSelectionRect();

        fRefreshOperation.Queue();
    }

    // callback from queued operation
    void TestApp::OnRefresh()
    {
        PerformRefresh();
    }

    // callback from a too early refresh operation
    void TestApp::OnRefreshTimer()
    {
        using namespace std::chrono;
        OIVCommands::Refresh();
        fLastRefreshTime = high_resolution_clock::now();
    }

    void TestApp::OnPreserveSelectionRect()
    {
        LoadImageSpaceSelection();
    }

    HWND TestApp::GetWindowHandle() const
    {
        return fWindow.GetHandle();
    }

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

    void TestApp::UpdateTitle()
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

    void TestApp::UnloadOpenedImaged()
    {
        if (fFileSessionController != nullptr)
            fFileSessionController->InvalidateCurrent();
        fImageState.ClearAll();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void TestApp::DeleteOpenedFile(bool permanently)
    {
        size_t stringLength = GetOpenedFileName().length();
        auto buffer = std::make_unique<wchar_t[]>(stringLength + 2);

        memcpy(buffer.get(), GetOpenedFileName().c_str(), (stringLength + 1) * sizeof(wchar_t));

        buffer.get()[stringLength + 1] = '\0';

        SHFILEOPSTRUCT file_op = {GetWindowHandle(),
                                  FO_DELETE,
                                  buffer.get(),
                                  nullptr,
                                  static_cast<FILEOP_FLAGS>(permanently ? 0 : FOF_ALLOWUNDO),
                                  FALSE,
                                  nullptr,
                                  nullptr};

        auto fileNameToRemove = GetOpenedFileName();
        fRequestedFileForRemoval = fileNameToRemove;

        int shResult = SHFileOperation(&file_op);

        if (shResult != 0)
        {
            // handle error
        }
        else
        {
            ProcessRemovalOfOpenedFile(fileNameToRemove);
        }
    }

    void TestApp::RefreshImage()
    {
        fRefreshOperation.Begin();
        fImageState.Refresh();
        fRefreshOperation.End(true);
    }

    void TestApp::DisplayOpenedFileName()
    {
        if (IsOpenedImageIsAFile())
            SetUserMessage(ViewerPresentationPolicy::FormatOpenedFileMessage(
                               MessageFormatter::FormatFilePath(GetOpenedFileName())),
                           static_cast<GroupID>(UserMessageGroups::SuccessfulFileLoad),
                           MessageFlags::Interchangeable | MessageFlags::Moveable);
    }

    void TestApp::AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        // add an image to a windows control, so create a system compatiable image - flipped BGRA  in windows.

        // Convert to BGRA bitmap.
        auto bgraImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                          false);

        // Flip vertically.
        bgraImage = IMUtil::ImageUtil::Transform({IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical},
                                                 bgraImage);

        // Create 32 bit BGRA color image

        ::Win32::BitmapBuffer bitmapBuffer{};
        bitmapBuffer.bitsPerPixel = bgraImage->GetBitsPerTexel();
        bitmapBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(bgraImage->GetRowPitchInBytes(), sizeof(DWORD));
        LLUtils::Buffer colorBuffer(bgraImage->GetHeight() * bitmapBuffer.rowPitch);
        bitmapBuffer.buffer = colorBuffer.data();
        bitmapBuffer.height = bgraImage->GetHeight();
        bitmapBuffer.width = bgraImage->GetWidth();

        // Create 24 bit mask image.
        ::Win32::BitmapBuffer maskBuffer{};
        maskBuffer.bitsPerPixel = 24;
        maskBuffer.height = bgraImage->GetHeight();
        maskBuffer.width = bgraImage->GetWidth();
        maskBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(maskBuffer.width * maskBuffer.bitsPerPixel / CHAR_BIT,
                                                                sizeof(DWORD));
        LLUtils::Buffer maskPixelsBuffer(maskBuffer.height * maskBuffer.rowPitch);
        maskBuffer.buffer = maskPixelsBuffer.data();

#pragma pack(push, 1)

        struct Color32
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
            uint8_t A;
        };

        struct Color24
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
        };
#pragma pack(pop)
        for (uint32_t l = 0; l < maskBuffer.height; l++)
        {
            const uint32_t sourceOffset = l * bgraImage->GetRowPitchInBytes();
            const uint32_t colorOffset = l * bitmapBuffer.rowPitch;
            const uint32_t maskOffset = l * maskBuffer.rowPitch;
            // Create mask/color pairs for GDI painting.
            for (size_t x = 0; x < maskBuffer.width; x++)
            {
                Color24& destMask = reinterpret_cast<Color24*>(reinterpret_cast<uint8_t*>(maskPixelsBuffer.data()) +
                                                               maskOffset)[x];
                Color32& destImage = reinterpret_cast<Color32*>(reinterpret_cast<uint8_t*>(colorBuffer.data()) +
                                                                colorOffset)[x];
                const Color32& sourceColor = reinterpret_cast<const Color32*>(
                    reinterpret_cast<const uint8_t*>(bgraImage->GetBuffer()) + sourceOffset)[x];

                const uint8_t AlphaChannel = sourceColor.A;
                const uint8_t invAlpha = 0xFF - AlphaChannel;
                // Mask is painted using ot OR operation.
                // White is fully opaque, black is fully transparent
                destMask.R = AlphaChannel;
                destMask.G = AlphaChannel;
                destMask.B = AlphaChannel;

                // Color image is painted using AND operation
                // Blend inverse alpha to adjust pixel color accoring to alpha.
                destImage.R = sourceColor.R | invAlpha;
                destImage.G = sourceColor.G | invAlpha;
                destImage.B = sourceColor.B | invAlpha;
            }
        }

        std::wstringstream ss;
        ss << imageSlot + 1 << L'/' << totalImages << L"  " << bitmapBuffer.width << L" x " << bitmapBuffer.height
           << L" x " << bitmapBuffer.bitsPerPixel << L" BPP";

        fWindow.GetImageControl().GetImageList().SetImage(
            {imageSlot, ss.str(), std::make_shared<::Win32::BitmapSharedPtr::element_type>(bitmapBuffer),
             std::make_shared<::Win32::BitmapSharedPtr::element_type>(maskBuffer)});
    }

    void TestApp::OnContextMenuTimer()
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

    bool TestApp::IsSubImagesVisible() const
    {
        using namespace IMCodec;
        if (fImageState.GetOpenedImage() != nullptr)
        {
            const auto mainImage = fImageState.GetOpenedImage()->GetImage();
            return mainImage != nullptr &&
                   SubImagePolicy::IsVisible(mainImage->GetSubImageGroupType(), mainImage->GetNumSubImages());
        }
        else
        {
            return false;
        }
    }

    void TestApp::LoadSubImages()
    {
        using namespace IMCodec;
        auto mainImage = fImageState.GetOpenedImage();
        auto numSubImages = mainImage->GetImage()->GetNumSubImages();
        if (IsSubImagesVisible())
        {
            const auto isMainAnActualImage = SubImagePolicy::IncludeMainImage(mainImage->GetImage()->GetItemType());
            const uint16_t totalImages =
                SubImagePolicy::TotalDisplayedImages(mainImage->GetImage()->GetNumSubImages(), isMainAnActualImage);
            // Add the first image.
            uint16_t currentImage = 0;
            if (isMainAnActualImage)
                AddImageToControl(mainImage->GetImage(), static_cast<uint16_t>(currentImage++), totalImages);

            std::vector<uint64_t> subImagePixels;
            subImagePixels.reserve(numSubImages);
            for (uint16_t i = 0; i < numSubImages; i++)
            {
                auto currentSubImage = mainImage->GetImage()->GetSubImage(i);
                subImagePixels.push_back(currentSubImage->GetTotalPixels());
                AddImageToControl(currentSubImage, static_cast<uint16_t>(currentImage++), totalImages);
            }
            // Reset selected sub image when loading new set of subimages
            const int selectionIndex =
                fDisplayBiggestSubImageOnLoad
                    ? SubImagePolicy::InitialSelectionIndex(isMainAnActualImage,
                                                            mainImage->GetImage()->GetTotalPixels(),
                                                            subImagePixels)
                    : SubImagePolicy::MainImageIndex;
            fWindow.GetImageControl().GetImageList().SetSelected(selectionIndex);
            fWindow.GetImageControl().RefreshScrollInfo();
        }
        else
        {
            fWindow.GetImageControl().GetImageList().Clear();
        }
    }

    bool TestApp::LoadFile(std::wstring filePath, IMCodec::PluginTraverseMode loaderFlags)
    {
        const auto clientSize = fWindow.GetClientSize();
        return ProcessImageLoadResult(fImageLoadController->LoadFile(
            filePath,
            loaderFlags,
            ImageLoadContext{static_cast<int>(clientSize.cx), static_cast<int>(clientSize.cy)}));
    }

    bool TestApp::ProcessImageLoadResult(const ImageLoadResult& loadResult)
    {
        auto formattedFilePath = MessageFormatter::FormatFilePath(loadResult.normalizedPath) + L"<textcolor=#ff8930>";
        const ImageLoadPresentation presentation =
            ImageLoadPresentationPolicy::Decide(loadResult, formattedFilePath);

        if (presentation.shouldLoadImage)
            LoadOivImage(loadResult.image);

        if (presentation.shouldShowMessage)
            SetUserMessage(presentation.message,
                           static_cast<GroupID>(UserMessageGroups::FailedFileLoad),
                           MessageFlags::Persistent);

        return presentation.succeeded;
    }

    void TestApp::LoadOivImage(OIVBaseImageSharedPtr oivImage)
    {
        // Enter this function only from the main thread.
        assert("TestApp::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());

        if (oivImage->GetImage() == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Expected a valid image");

        fFileDisplayTimer.Start();

        fCurrentFrame = 0;
        fCurrentSequencerSpeed = 1.0;
        fQueueImageInfoLoad = GetImageInfoVisible();
        SetImageInfoVisible(false);
        SetResamplingEnabled(false);
        fImageState.SetOpenedImage(oivImage);

        fRefreshOperation.Begin();

        fImageState.ResetUserState();

        if (fResetTransformationMode == ResetTransformationMode::ResetAll)
            FitToClientAreaAndCenter();

        AutoPlaceImage();

        fImageState.Refresh();
        fWindow.SetShowImageControl(IsSubImagesVisible());
        UnloadWelcomeMessage();
        DisplayOpenedFileName();

        if (fContextMenu != nullptr)
        {
            fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"),
                                     oivImage->GetImageSource() == ImageSource::File);
            fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"),
                                     oivImage->GetImageSource() == ImageSource::File);
        }

        fRefreshOperation.End();
        fFileDisplayTimer.Stop();

        LoadSubImages();

        fImageState.GetOpenedImage()->SetDisplayTime(
            fFileDisplayTimer.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds));

        fLastImageLoadTimeStamp.Start();

        if (fIsTryToLoadInitialFile == true)
        {
            fWindow.SetVisible(true);
            fIsTryToLoadInitialFile = false;
        }
        else
        {
            ProcessLoadedDirectory();
        }

        UpdateOpenImageUI();

        SetResamplingEnabled(true);

        if (fQueueImageInfoLoad == true)
        {
            SetImageInfoVisible(true);
            fQueueImageInfoLoad = false;
        }

        // if sub images of main image are animation frame, start sequencer, otherwise make sure it's stopped
        fSequencerTimer.SetInterval(fImageState.GetOpenedImage()->GetImage()->GetSubImageGroupType() ==
                                            IMCodec::ImageItemType::AnimationFrame
                                        ? 1
                                        : 0);
    }

    void TestApp::UpdateOpenImageUI()
    {
        if (IsImageOpen())
        {
            UpdateTitle();
            fVirtualStatusBar.SetText("imageDescription",
                                      fImageState.GetImage(ImageChainStage::Deformed)->GetDescription());
        }
    }

    const std::wstring& TestApp::GetOpenedFileName() const
    {
        static const std::wstring emptyString;
        std::shared_ptr<OIVFileImage> file = std::dynamic_pointer_cast<OIVFileImage>(fImageState.GetOpenedImage());
        return file != nullptr ? file->GetFileName() : emptyString;
    }

    bool TestApp::IsImageOpen() const
    {
        return fImageState.GetOpenedImage() != nullptr;
    }

    bool TestApp::IsOpenedImageIsAFile() const
    {
        return fImageState.GetOpenedImage() != nullptr &&
               fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File;
    }

    void TestApp::SortFileList()
    {
        if (fFileSessionController != nullptr)
            fFileSessionController->SortFileList();
    }

    void TestApp::LoadFileInFolder(std::wstring absoluteFilePath)
    {
        if (fFileSessionController != nullptr)
            fFileSessionController->LoadFileInFolder(absoluteFilePath);
    }

    void TestApp::OnScroll(const LLUtils::PointF64& panAmount)
    {
        Pan(panAmount);

        const PanCursorHint cursorHint = InputGesturePolicy::CursorHintForPan(panAmount);
        const Win32::MainWindow::CursorType cursorType =
            cursorHint.sizeAll ? Win32::MainWindow::CursorType::SizeAll
                               : static_cast<Win32::MainWindow::CursorType>(cursorHint.directionIndex + 2);
        fWindow.SetCursorType(cursorType);
    }

    void TestApp::DelayResamplingCallback()
    {
        fTimerNoActiveZoom.SetInterval(0);
        fImageState.SetResample(true);
        fImageState.Refresh();
        fRefreshOperation.Queue();
    }

    void TestApp::Init(std::wstring relativeFilePath)
    {
        using namespace std;
        using namespace placeholders;

        wstring filePath = LLUtils::FileSystemHelper::ResolveFullPath(relativeFilePath);
        filePath = std::filesystem::path(filePath).lexically_normal();

        const bool isDirectory = std::filesystem::is_directory(filePath);

        const bool isInitialFileProvided = filePath.empty() == false && isDirectory == false;
        const bool isInitialFileExists = isInitialFileProvided && filesystem::exists(filePath);

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
                                           std::bind(&TestApp::OnScroll, this, std::placeholders::_1)};
        fAutoScroll = std::make_unique<AutoScroll>(params);

        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));
        fWindow.GetCanvasWindow().AddEventListener(std::bind(&TestApp::HandleClientWindowMessages, this, _1));

        fRefreshOperation.Begin();

        fTimerNoActiveZoom.SetTargetWindow(fWindow.GetHandle());

        fTimerNoActiveZoom.SetCallback(std::bind(&TestApp::DelayResamplingCallback, this));

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
                    currentImage->GetAnimationData().delayMilliseconds,
                    fCurrentSequencerSpeed));
                fCurrentFrame = SequencerPolicy::NextFrame(fCurrentFrame,
                                                           fImageState.GetOpenedImage()->GetImage()->GetNumSubImages());
                RefreshImage();
            });

        fMessageManager = std::make_unique<MessageManager>(fWindow.GetHandle(), &fLabelManager, 5,
                                                           [&]() -> void { fRefreshOperation.Queue(); });

        OIVCommands::Init(fWindow.GetCanvasHandle());

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
            SetUserMessage(L"Can not load the file: "s + filePath + L", it doesn't exist"s,
                           static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
        }

        fRefreshOperation.End(!isInitialFileLoadedSuccesfuly);

        if (isInitialFileLoadedSuccesfuly)
        {
            LoadOivImage(fInitialFile);
            fInitialFile.reset();
        }
    }

    IMCodec::ImageSharedPtr TestApp::GetImageByIndex(int32_t index)
    {
        using namespace IMCodec;
        auto openedImage = fImageState.GetOpenedImage()->GetImage();

        const auto isMainAnActualImage = SubImagePolicy::IncludeMainImage(openedImage->GetItemType());
        const auto actualIndex = SubImagePolicy::ActualImageIndexFromDisplayIndex(index, isMainAnActualImage);

        if (actualIndex == SubImagePolicy::MainImageIndex)
        {
            return openedImage;
        }
        else
        {
            return openedImage->GetSubImage(actualIndex);
        }
    }

    void TestApp::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        auto imageIndex = ImageSelectionChangeArgs.imageIndex;
        if (imageIndex >= 0)
        {
            auto image = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, GetImageByIndex(imageIndex));

            fImageState.SetImageChainRoot(image);
            fRefreshOperation.Begin();
            FitToClientAreaAndCenter();
            RefreshImage();

            if (GetImageInfoVisible() == true)
                ShowImageInfo();

            UpdateOpenImageUI();

            fRefreshOperation.End();
        }
    }

    void TestApp::ProcessRemovalOfOpenedFile(const std::wstring& fileName)
    {
        const bool removeInternalDeletes =
            (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedInternally) ==
            DeletedFileRemovalMode::DeletedInternally;
        const bool removeExternalDeletes =
            (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedExternally) ==
            DeletedFileRemovalMode::DeletedExternally;
        const size_t fileCount =
            fFileSessionController != nullptr ? fFileSessionController->GetFileList().GetSize() : 0;

        const auto action = FileRemovalPolicy::Decide(GetOpenedFileName(),
                                                      fileName,
                                                      fRequestedFileForRemoval,
                                                      removeInternalDeletes,
                                                      removeExternalDeletes,
                                                      fileCount);
        if (action != RemovedFileAction::Ignore)
        {
            bool firstJumpSucceeded = false;
            bool fallbackJumpSucceeded = false;

            if (action == RemovedFileAction::TryStart)
            {
                firstJumpSucceeded = JumpFiles(FileList::IndexStart);
            }
            else if (action == RemovedFileAction::TryNextThenPrevious)
            {
                firstJumpSucceeded = JumpFiles(1);
                if (!firstJumpSucceeded)
                    fallbackJumpSucceeded = JumpFiles(-1);
            }

            if (FileRemovalPolicy::ShouldUnloadAfterJumps(action, firstJumpSucceeded, fallbackJumpSucceeded))
            {
                UnloadOpenedImaged();
                ShowWelcomeMessage();
            }

            fRequestedFileForRemoval = {};
        }

        UpdateTitle();
    }

    void TestApp::OnFileChangedImpl(const FileWatcher::FileChangedEventArgs* fileChangedEventArgsPtr)
    {
        auto fileChangedEventArgs = *fileChangedEventArgsPtr;
        const bool hasActiveFolder = fFileSessionController != nullptr;
        const auto activeFolderID =
            hasActiveFolder ? fFileSessionController->GetFileList().GetFolderID() : IFileWatcher::FolderID{};

        switch (FileChangePolicy::Decide(fileChangedEventArgs,
                                         hasActiveFolder,
                                         activeFolderID,
                                         fCOnfigurationFolderID,
                                         std::filesystem::path(GetOpenedFileName()).wstring()))
        {
            case FileChangeAction::CurrentFileChanged:
                ProcessCurrentFileChanged();
                break;
            case FileChangeAction::ClearWatchedFolder:
                fCurrentFolderWatched.clear();
                break;
            case FileChangeAction::ReloadSettings:
                LoadSettings();
                break;
            case FileChangeAction::Ignore:
                break;
            case FileChangeAction::UnexpectedFolder:
                LL_EXCEPTION_UNEXPECTED_VALUE;
                break;
        }
    }

    void TestApp::OnFileChanged(FileWatcher::FileChangedEventArgs fileChangedEventArgs)
    {
        fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(InterThreadMessages::FileChanged),
                           fileChangedEventArgs);
    }

    void TestApp::OnMouseEvent(const LInput::ButtonStdExtension<MouseButtonType>::ButtonEvent& btnEvent)
    {
        using namespace LInput;
        bool isMouseCursorOnTopOfWindowAndInsideClientRect = fWindow.IsUnderMouseCursor() &&
                                                             fWindow.IsMouseCursorInClientRect();
        if (btnEvent.button == MouseButton::Middle && btnEvent.eventType == EventType::Pressed &&
            isMouseCursorOnTopOfWindowAndInsideClientRect)
        {
            fAutoScroll->ToggleAutoScroll();
            if (fAutoScroll->IsAutoScrolling() == false)
            {
                fWindow.SetCursorType(::OIV::Win32::MainWindow::CursorType::SystemDefault);
                fAutoScrollAnchor.reset();
            }
            else
            {
                std::wstring anchorPath = LLUtils::StringUtility::ToNativeString(
                                              LLUtils::PlatformUtility::GetExeFolder()) +
                                          L"./Resources/Cursors/arrow-C.cur";
                std::unique_ptr<OIVFileImage> fileImage = std::make_unique<OIVFileImage>(anchorPath);
                if (fileImage->Load(&fImageLoader, IMCodec::PluginTraverseMode::AnyPlugin) == RC_Success)
                {
                    fileImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
                    fileImage->SetPosition(static_cast<LLUtils::PointF64>(
                        static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) -
                        static_cast<LLUtils::PointI32>(fileImage->GetImage()->GetDimensions()) / 2));
                    fileImage->SetScale({1.0, 1.0});
                    fileImage->SetOpacity(0.5);
                    fileImage->SetVisible(true);
                    fAutoScrollAnchor = std::move(fileImage);
                    // TODO: do we need update here when loading the cursor anchor ?
                }
            }
        }

        if (btnEvent.button == MouseButton::Left && btnEvent.eventType == EventType::Released)
        {
            fWindow.SetLockMouseToWindowMode(::Win32::LockMouseToWindowMode::NoLock);
        }

        using namespace ::Win32;
        LockMouseToWindowMode lockMode = LockMouseToWindowMode::NoLock;
        const auto& mouseState = fMouseDevicesState.find(btnEvent.parent->GetID())->second;
        const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;
        const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
        const bool IsRightCatured = fMouseCaptureState.IsCaptured(MouseButtonType::Right);

        if (btnEvent.button == MouseButton::Left)
        {
            if (btnEvent.eventType == EventType::Pressed && IsRightDown &&
                isMouseCursorOnTopOfWindowAndInsideClientRect)
            {
                // Rocker gesture - navigate backward
                fRockerGestureActivate = true;
                fContextMenuTimer.SetInterval(0);
                JumpFiles(-1);
            }
            else if (IsRightDown == false && IsRightCatured == false)
            {
                // Window drag and resize
                if (Win32Helper::IsKeyPressed(VK_MENU) == false && fWindow.IsFullScreen() == false)
                {
                    if (Win32Helper::IsKeyPressed(VK_CONTROL) == true)
                        lockMode = LockMouseToWindowMode::LockResize;
                    else
                        lockMode = LockMouseToWindowMode::LockMove;
                }
            }
            if (btnEvent.eventType == EventType::Released)
            {
                lockMode = LockMouseToWindowMode::NoLock;
            }

            fWindow.SetLockMouseToWindowMode(lockMode);

            if (Win32Helper::IsKeyPressed(VK_MENU))
            {
                SelectionRect::Operation op = SelectionRect::Operation::NoOp;
                if (btnEvent.eventType == EventType::Pressed && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::BeginDrag;
                else if (btnEvent.eventType == EventType::Released && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::EndDrag;

                fSelectionRect.SetSelection(op, SnapToScreenSpaceImagePixels(fWindow.GetMousePosition()));
                SaveImageSpaceSelection();
            }
        }
        if (btnEvent.button == MouseButton::Back || btnEvent.button == MouseButton::Forward)
        {
            if (btnEvent.eventType == EventType::Pressed && fWindow.IsUnderMouseCursor())
            {
                fTimerNavigation.SetInterval(fQuickBrowseDelay);
            }
            else
            {
                if (fMouseCaptureState.IsCaptured(MouseButton::Back) == false &&
                    fMouseCaptureState.IsCaptured(MouseButton::Forward) == false)
                    fTimerNavigation.SetInterval(0);
            }
        }

        if (btnEvent.button == MouseButton::Right && btnEvent.eventType == EventType::Pressed &&
            isMouseCursorOnTopOfWindowAndInsideClientRect)
        {
            // Rocker gesture - navigate forward
            if (IsLeftDown)
            {
                fRockerGestureActivate = true;
                fContextMenuTimer.SetInterval(0);
                JumpFiles(1);
            }

            if (fContextMenuTimer.GetInterval() == 0 && fRockerGestureActivate == false)
            {
                fContextMenuTimer.SetInterval(500);
                fDownPosition = ::Win32::Win32Helper::GetMouseCursorPosition();
            }
        }
    }

    void TestApp::OnMouseInput(const LInput::RawInput::RawInputEventMouse& mouseInput)
    {
        using namespace LInput;
        using namespace ::Win32;

        const auto& mouseState = fMouseDevicesState.find(mouseInput.deviceIndex)->second;

        // const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
        const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;

        const bool IsRightCatured = fMouseCaptureState.IsCaptured(MouseButtonType::Right);
        const bool IsLeftCaptured = fMouseCaptureState.IsCaptured(MouseButtonType::Left);
        // const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        // const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::Released;

        const bool isMouseUnderCursor = fWindow.IsUnderMouseCursor();

        // Quick browse feature
        // const bool isNavigationBackwardDown = (mouseState.GetButtonState(MouseButtonType::Back) ==
        // ButtonState::Down); const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseButtonType::Back) ==
        // ButtonState::Up); const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseState::Button::Third)
        // == MouseState::State::Up); const bool isNavigationForwardDown =
        // mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Down; const bool isNavigationForwardUp =
        // mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Up;

        // Selection rect
        if (Win32Helper::IsKeyPressed(VK_MENU))
        {
            if (IsLeftCaptured)
            {
                auto snappedPOsition = SnapToScreenSpaceImagePixels(fWindow.GetMousePosition());
                fSelectionRect.SetSelection(SelectionRect::Operation::Drag, snappedPOsition);
                SaveImageSpaceSelection();
            }
        }

        if (IsRightCatured == true && fContextMenu->IsVisible() == false)
        {
            if (mouseInput.deltaX != 0 || mouseInput.deltaY != 0)
                Pan(LLUtils::PointF64(mouseInput.deltaX, mouseInput.deltaY));
        }

        LONG wheelDelta = mouseInput.wheelDelta;

        if (wheelDelta != 0)
        {
            // Browse files
            if (isMouseUnderCursor && Win32Helper::IsKeyPressed(VK_MENU))
            {
                ExecutePredefinedCommand(wheelDelta > 0 ? "PreviousSubImage" : "NextSubImage");
            }
            else if (isMouseUnderCursor && Win32Helper::IsKeyPressed(VK_SHIFT))
            {
                ExecutePredefinedCommand(wheelDelta > 0 ? "PreviousImageInFolder" : "NextImageInFolder");
            }
            else if (IsRightCatured || isMouseUnderCursor)
            {
                POINT mousePos = fWindow.GetMousePosition();
                // 20% percent zoom in each wheel step
                if (IsRightCatured)
                    //  Zoom to center of the client area if currently panning.
                    Zoom(wheelDelta * 0.2);
                else
                    Zoom(wheelDelta * 0.2, mousePos.x, mousePos.y);
            }
        }

        if (IsRightDown)
        {
            LLUtils::PointI32 currentPosition = Win32Helper::GetMouseCursorPosition();
            if (currentPosition.DistanceSquared(fDownPosition) > 25)
                fContextMenuTimer.SetInterval(0);
        }
        else
        {
            fContextMenuTimer.SetInterval(0);
        }

        if (IsLeftDown == false && IsRightDown == false)
            fRockerGestureActivate = false;
    }
    void TestApp::OnRawInput(const LInput::RawInput::RawInputEvent& evnt)
    {
        using namespace LInput;
        if (evnt.deviceType == RawInput::RawInputDeviceType::Mouse)
        {
            const auto& mouseEvent = static_cast<const RawInput::RawInputEventMouse&>(evnt);

            // Add button states for multiple mouses.
            auto it = fMouseDevicesState.find(evnt.deviceIndex);
            // if mouse ID not found add new buttonstates entry.
            if (it == std::end(fMouseDevicesState))
            {
                it = fMouseDevicesState.emplace(evnt.deviceIndex, decltype(fMouseDevicesState)::mapped_type()).first;
                // Add standard extension
                auto stdExtension = std::make_shared<ButtonStdExtension<MouseButtonType>>(evnt.deviceIndex, 250, 0);
                stdExtension->OnButtonEvent.Add(std::bind(&TestApp::OnMouseEvent, this, std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(stdExtension));

                // Add multitap extension for click, double click and triple click
                /*
                auto multitapextension = std::make_shared<MultitapExtension<MouseButtonType>>(evnt.deviceIndex, 500, 2);
                multitapextension->OnButtonEvent.Add(std::bind(&TestApp::OnMouseMultiTap, this,std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(multitapextension));
                */
            }

            for (size_t i = 0; i < RawInput::MaxMouseButtons; i++)
            {
                it->second.SetButtonState(
                    static_cast<decltype(fMouseDevicesState)::mapped_type::underlying_button_type>(i),
                    mouseEvent.buttonState[i]);

                fMouseCaptureState.Update(static_cast<MouseButton>(i),
                                           mouseEvent.buttonState[i],
                                           fWindow.IsUnderMouseCursor());

                fMouseClickEventHandler.SetButtonState(static_cast<MouseButton>(i), mouseEvent.buttonState[i]);
            }

            fMouseClickEventHandler.SetMouseDelta(mouseEvent.deltaX, mouseEvent.deltaY);

            OnMouseInput(mouseEvent);
        }
    }

    void TestApp::OnMouseMultiClick(const MouseMultiClickHandler::EventArgs& args)
    {
        using namespace LInput;
        if (args.clickCount == 2 && fWindow.IsMouseCursorInClientRect() && fWindow.IsUnderMouseCursor())
        {
            const auto& mouseState = fMouseDevicesState.begin()->second;
            const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;
            const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;

            if (args.button == MouseButton::Left)
            {
                if (IsRightDown == false)
                {
                    if (fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
                    {
                        CancelSelection();
                    }
                    else
                    {
                        ToggleFullScreen(::Win32::Win32Helper::IsKeyPressed(VK_MENU) ? true : false);
                    }
                }
            }

            if (args.button == MouseButton::Right)
            {
                if (IsLeftDown == false)
                {
                    ExecutePredefinedCommand("PasteImageFromClipboard");
                }
            }
        }
    }

    void TestApp::ProcessLoadedDirectory()
    {
        if (IsOpenedImageIsAFile())
        {
            LoadFileInFolder(GetOpenedFileName());
        }
    }

    void TestApp::OnFileIndexResidencyReady(const std::wstring& fileName, IMCodec::ImageSharedPtr image)
    {
        if (image == nullptr)
            return;

        if (fFileSessionController == nullptr || !fFileSessionController->IsCurrentFile(fileName))
        {
            return;
        }

        std::shared_ptr<OIVFileImage> file = std::make_shared<OIVFileImage>(fileName, std::move(image));
        LoadOivImage(file);
    }

    void TestApp::OnFolderLoadResidencyReady(const BrowseResidencyManager::FileListSnapshot& snapshot,
                                             const std::wstring& fileName,
                                             IMCodec::ImageSharedPtr image)
    {
        if (image == nullptr)
            return;

        if (fFileSessionController == nullptr ||
            !fFileSessionController->OnFolderLoadResidencyReady(snapshot, fileName, image))
        {
            return;
        }
    }

    void TestApp::PostInitOperations()
    {
        LLUtils::Logger::GetSingleton().AddLogTarget(&mLogFile);

        fTimerTopMostRetention.SetTargetWindow(fWindow.GetHandle());
        fTimerTopMostRetention.SetCallback([this]() { ProcessTopMost(); });

        fTimerSlideShow.SetTargetWindow(fWindow.GetHandle());
        fTimerSlideShow.SetCallback(
            [this]()
            {
                SetSlideShowEnabled(false);

                if (fFileSessionController == nullptr)
                    return;

                const auto& fileList = fFileSessionController->GetFileList();
                bool foundFile = JumpFiles(1) ||
                                 (fSlideshowPolicy.ShouldWrap(fileList.GetCurrentIndex(), fileList.GetSize()) &&
                                  JumpFiles(FileList::IndexStart));

                SetSlideShowEnabled(foundFile);
            });

        fDoubleTap.callback = [this]()
        {
            fWindow.SetAlwaysOnTop(true);
            fTopMostCounter = 3;
            SetTopMostUserMesage();
            fTimerTopMostRetention.SetInterval(1000);
        };

        const ImageFormatCatalog imageFormatCatalog =
            ImageFormatCatalogPolicy::Build(fImageLoader.GetImageCodec().GetPluginsInfo());

        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters readFilters;
        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters writeFilters;

        for (const ImageFormatFilter& filter : imageFormatCatalog.readFilters)
            readFilters.push_back({filter.description, filter.extensions});

        for (const ImageFormatFilter& filter : imageFormatCatalog.writeFilters)
            writeFilters.push_back({filter.description, filter.extensions});

        fKnownFileTypesSet = imageFormatCatalog.knownFileTypesSet;
        fKnownFileTypes = imageFormatCatalog.knownFileTypes;
        fDefaultSaveFileExtension = imageFormatCatalog.defaultSaveFileExtension;
        fDefaultSaveFileFormatIndex = imageFormatCatalog.defaultSaveFileFormatIndex;

        fOpenComDlgFilters = {readFilters};
        fSaveComDlgFilters = {writeFilters};

        fFileWatcher.FileChangedEvent.Add(std::bind(&TestApp::OnFileChanged, this, std::placeholders::_1));

        // If a file has been succesfuly loaded, index all the file in the folder

        // IFileListProvider* fileListProvider, FileWatcher* fileWatcher, FileSorter fileSorter,
        //          FileListStringSetType knownnFileTypesSet, FileListStringType knownFileTypes

        fFileSessionController = std::make_unique<FileSessionController>(
            this,
            &fFileWatcher,
            &fFileSorter,
            fKnownFileTypesSet,
            fKnownFileTypes,
            fImageResidency,
            [this](const std::wstring& fileName, IMCodec::ImageSharedPtr image)
            {
                if (!fIsShuttingDown)
                {
                    fEventSync.AddData(
                        static_cast<std::underlying_type_t<InterThreadMessages>>(
                            InterThreadMessages::FileIndexResidencyReady),
                        FileIndexResidencyReadyData{fileName, image});
                }
            },
            [this](const BrowseResidencyManager::FileListSnapshot& snapshot,
                   const std::wstring& fileName,
                   IMCodec::ImageSharedPtr image)
            {
                if (!fIsShuttingDown)
                {
                    fEventSync.AddData(
                        static_cast<std::underlying_type_t<InterThreadMessages>>(
                            InterThreadMessages::FolderLoadResidencyReady),
                        FolderLoadResidencyReadyData{snapshot, fileName, image});
                }
            });
        fImageLoadController->SetFileSessionController(fFileSessionController.get());

        ProcessLoadedDirectory();
        UpdateTitle();

        AddCommandsAndKeyBindings();

        fWindow.GetImageControl().GetImageList().ImageSelectionChanged.Add(
            std::bind(&TestApp::OnImageSelectionChanged, this, std::placeholders::_1));

        // renderer took over on the window, no need to erase background.
        fWindow.GetCanvasWindow().SetEraseBackground(false);

        fContextMenuTimer.SetTargetWindow(fWindow.GetHandle());
        fContextMenuTimer.SetCallback(std::bind(&TestApp::OnContextMenuTimer, this));
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
            std::bind(&TestApp::OnNotificationIcon, this, std::placeholders::_1));

        fNotificationContextMenu = std::make_unique<ContextMenu<int>>(fWindow.GetHandle());
        fNotificationContextMenu->AddItem(OIV_TEXT("Quit"), int{});

        using namespace LInput;
        fRawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls,
                            RawInput::GenericDesktopControlsUsagePage::Mouse, RawInput::Flags::EnableBackground);

        fRawInput.OnInput.Add(std::bind(&TestApp::OnRawInput, this, std::placeholders::_1));
        fRawInput.Enable(true);

        fMouseClickEventHandler.OnMouseClickEvent.Add(
            std::bind(&TestApp::OnMouseMultiClick, this, std::placeholders::_1));

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

    void TestApp::OnSettingChange(const std::wstring& key, const std::wstring& value)
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

    void TestApp::LoadSettings()
    {
        auto settings = ConfigurationLoader::LoadSettings();
        for (const auto& pair : settings)
            OnSettingChange(LLUtils::StringUtility::ToWString(pair.first),
                            LLUtils::StringUtility::ToWString(pair.second));
    }

    void TestApp::OnNotificationIcon(::Win32::NotificationIconGroup::NotificationIconEventArgs args)
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

    OIV_Filter_type TestApp::GetFilterType() const
    {
        return fImageState.GetVisibleImage()->GetFilterType();
    }

    void TestApp::UpdateExposure()
    {
        OIVCommands::ExecuteCommand(OIV_CMD_ColorExposure, &fColorExposure, &NullCommand);
        fRefreshOperation.Queue();
    }

    bool TestApp::ToggleColorCorrection()
    {
        bool isDefault = true;
        if (memcmp(&fColorExposure, &DefaultColorCorrection, sizeof(OIV_CMD_ColorExposure_Request)) == 0)
        {
            std::swap(fLastColorExposure, fColorExposure);
        }
        else
        {
            isDefault = false;
            std::swap(fLastColorExposure, fColorExposure);
            fColorExposure = DefaultColorCorrection;
        }
        UpdateExposure();

        return isDefault;
    }

    void TestApp::OnCountingColorsCompleted(const CountColorsData& countColorsData)
    {
        fIsColorThreadRunning = false;

        switch (ColorCountPolicy::DecideCompletion(countColorsData.image,
                                                   fImageState.GetImage(ImageChainStage::SourceImage).get(),
                                                   GetImageInfoVisible()))
        {
            case ColorCountCompletionAction::ApplyToCurrentImage:
                // Still the same image on display, assing number of colors and refresh ImageInfo

                // if counting unique colors has failed, assign UniqueColorsFailed, so counting colors won't
                // restart for this image.
                fCountingImageColor.reset();
                fImageState.GetImage(ImageChainStage::SourceImage)
                    ->SetNumUniqueColors(ColorCountPolicy::NormalizeCountResult(
                        countColorsData.colorCount,
                        UniqueColorsUninitialized - 1,
                        UniqueColorsFailed));

                if (GetImageInfoVisible() == true)
                    ShowImageInfo();
                break;
            case ColorCountCompletionAction::CountVisibleImage:
                // If a different image on display Just count colors
                CountColorsAsync();
                break;
            case ColorCountCompletionAction::Ignore:
                break;
        }
    }

    void TestApp::OnMessageFromBackgroundThread(const EventData& sharedData)
    {
        if (sharedData.data.has_value() == false)
            LL_EXCEPTION_UNEXPECTED_VALUE;

        switch (static_cast<InterThreadMessages>(sharedData.id))
        {
            case InterThreadMessages::FileChanged:
            {
                const FileWatcher::FileChangedEventArgs* fileChangedEventArgs =
                    std::any_cast<FileWatcher::FileChangedEventArgs>(&(sharedData.data));

                if (fileChangedEventArgs == nullptr)
                    LL_EXCEPTION_UNEXPECTED_VALUE;

                OnFileChangedImpl(fileChangedEventArgs);

                break;
            }
            case InterThreadMessages::FileIndexResidencyReady:
            {
                const auto& fileIndexResidencyReadyData = std::any_cast<const FileIndexResidencyReadyData&>(sharedData.data);
                OnFileIndexResidencyReady(fileIndexResidencyReadyData.fileName, fileIndexResidencyReadyData.image);
                break;
            }
            case InterThreadMessages::FolderLoadResidencyReady:
            {
                const auto& folderLoadResidencyReadyData = std::any_cast<const FolderLoadResidencyReadyData&>(sharedData.data);
                OnFolderLoadResidencyReady(folderLoadResidencyReadyData.snapshot, folderLoadResidencyReadyData.fileName,
                                           folderLoadResidencyReadyData.image);
                break;
            }
            case InterThreadMessages::AutoScroll:
                LL_EXCEPTION_NOT_IMPLEMENT("Auto scroll not implemented");
            case InterThreadMessages::CountColors:
            {
                const auto& colorsDAta = std::any_cast<const CountColorsData&>(sharedData.data);
                OnCountingColorsCompleted(colorsDAta);
                break;
            }

                LL_EXCEPTION_NOT_IMPLEMENT("Count colors not implemented");
            case InterThreadMessages::FirstFrameDisplayed:
                LL_EXCEPTION_NOT_IMPLEMENT("First frame displayed not implemented");
            case InterThreadMessages::LoadFileExternally:
                LL_EXCEPTION_NOT_IMPLEMENT("Load file externally not implemented");
            default:
                break;
        }
    }

    void TestApp::Run()
    {
        bool shouldQuit = false;
        while (!shouldQuit)
        {
            DWORD count = 1;
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

    bool TestApp::JumpFiles(FileList::index_type step)
    {
        return fFileSessionController != nullptr && fFileSessionController->JumpFiles(step);
    }

    void TestApp::ToggleFullScreen(bool multiFullScreen)
    {
        fRefreshOperation.Begin();
        fWindow.ToggleFullScreen(multiFullScreen);
        Center();
        fRefreshOperation.End();
    }

    void TestApp::ToggleBorders()
    {
        fShowBorders = !fShowBorders;
        {
            using namespace ::Win32;
            fWindow.SetWindowStyles(
                WindowStyle::ResizableBorder | WindowStyle::MaximizeButton | WindowStyle::MinimizeButton, fShowBorders);
        }
    }

    void TestApp::SetSlideShowEnabled(bool enabled)
    {
        if (fSlideshowPolicy.IsEnabled() != enabled)
        {
            fSlideshowPolicy.SetEnabled(enabled);
            fTimerSlideShow.SetInterval(fSlideshowPolicy.GetTimerIntervalMs());
        }
    }

    void TestApp::SetFilterLevel(OIV_Filter_type filterType)
    {
        fImageState.GetVisibleImage()->SetFilterType(
            std::clamp(filterType, FT_None, static_cast<OIV_Filter_type>(FT_Count - 1)));

        fRefreshOperation.Queue();
    }

    void TestApp::ToggleGrid()
    {
        fIsGridEnabled = !fIsGridEnabled;
        UpdateRenderViewParams();
    }

    void TestApp::UpdateRenderViewParams()
    {
        CmdRequestTexelGrid grid;
        grid.gridSize = fIsGridEnabled ? 1.0 : 0.0;
        grid.transparencyMode = fTransparencyMode;
        grid.generateMipmaps = fDownScalingTechnique == DownscalingTechnique::HardwareMipmaps;
        if (OIVCommands::ExecuteCommand(CE_TexelGrid, &grid, &NullCommand) == RC_Success)
        {
            fRefreshOperation.Queue();
        }
    }

    bool TestApp::handleKeyInput(const ::Win32::EventWinMessage* evnt)
    {
        LInput::KeyCombination keyCombination = LInput::KeyCombination::FromVirtualKey(
            static_cast<uint32_t>(evnt->message.wParam), static_cast<uint32_t>(evnt->message.lParam));
        LInput::KeyBindings<BindingElement>::ConcreteBindingType bindings;
        bool result = true;
        if (result == fKeyBindings.GetBinding(keyCombination, bindings))
        {
            for (const auto& binding : bindings)
                result |= ExecutePredefinedCommand(binding.commandDescription);
        }

        return result;
    }

    void TestApp::SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState)
    {
        fImageState.SetOffset(ResolveOffset(offset));
        fPreserveImageSpaceSelection.Queue();
        fRefreshOperation.Queue();

        if (preserveOffsetLockState == false)
            fIsOffsetLocked = false;
    }

    void TestApp::SetOriginalSize()
    {
        SetZoomInternal(1.0, -1, -1);
        Center();
    }

    void TestApp::Pan(const LLUtils::PointF64& panAmount)
    {
        if (fImageState.GetOpenedImage() != nullptr)
            SetOffset(panAmount * fDPIadjustmentFactor + fImageState.GetOffset());
    }

    void TestApp::Zoom(double amount, int zoomX, int zoomY)
    {
        if (IsImageOpen())
        {
            CommandManager::CommandRequest request;
            request.displayName = "Zoom";
            request.args = CommandManager::CommandArgs::FromString(
                "val=" + std::to_string(amount) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY));
            request.commandName = "cmd_zoom";
            ExecuteCommand(request);
        }
    }

    void TestApp::ZoomInternal(double amount, int zoomX, int zoomY)
    {
        const double adaptiveAmount = fAdaptiveZoom.Add(amount);
        const double adjustedAmount = ViewActionController::RelativeZoom(GetScale(), adaptiveAmount);
        SetZoomInternal(adjustedAmount, zoomX, zoomY);
    }

    void TestApp::FitToClientAreaAndCenter()
    {
        if (IsImageOpen())
        {
            using namespace LLUtils;
            SIZE clientSize = fWindow.GetCanvasSize();
            if (clientSize.cx > 0 && clientSize.cy > 0)  // window might minimized.
            {
                const double zoom = ViewTransformController::FitScale(PointF64(clientSize.cx, clientSize.cy),
                                                                       GetImageSize(ImageSizeType::Transformed));
                fRefreshOperation.Begin();
                fIsLockFitToScreen = true;
                SetZoomInternal(zoom, -1, -1, true);
                Center();
                fRefreshOperation.End();
            }
        }
    }

    LLUtils::PointF64 TestApp::GetImageSize(ImageSizeType imageSizeType)
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

    void TestApp::UpdateSelectionRectText()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        auto selectionSizeStr = SelectionWorkflowPolicy::FormatSelectionSize(fImageSpaceSelection);

        if (selectionSizeText == nullptr)
        {
            // Create new user message.
            selectionSizeText = fLabelManager.GetOrCreateTextLabel("selectionSizeText");
            selectionSizeText->SetFontPath(LabelManager::sFontPath);
            selectionSizeText->SetFontSize(11);
            selectionSizeText->SetBackgroundColor({0, 0, 0, 192});
            selectionSizeText->SetTextColor({170, 170, 170, 255});
            selectionSizeText->SetTextRenderMode(FreeType::RenderMode::Antialiased);
            selectionSizeText->SetOutlineWidth(0);

            selectionSizeText->SetFilterType(OIV_Filter_type::FT_None);
            selectionSizeText->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
            selectionSizeText->SetScale({1.0, 1.0});
            selectionSizeText->SetOpacity(1.0);
        }

        // text already exists, just make visible.
        selectionSizeText->SetVisible(true);
        selectionSizeText->SetText(selectionSizeStr);
        selectionSizeText->Create();

        const auto labelPosition = SelectionWorkflowPolicy::PlaceSelectionLabel(
            fSelectionRect.GetSelectionRect(),
            {static_cast<int32_t>(selectionSizeText->GetImage()->GetWidth()),
             static_cast<int32_t>(selectionSizeText->GetImage()->GetHeight())},
            static_cast<LLUtils::PointI32>(fWindow.GetClientSize()));

        selectionSizeText->SetPosition(static_cast<LLUtils::PointF64>(labelPosition));
    }

    void TestApp::OnImageReady(IMCodec::ImageSharedPtr image) {}

    LLUtils::PointI32 TestApp::SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen)
    {
        return SelectionWorkflowPolicy::SnapToImagePixels(pointOnScreen, GetScale(), GetOffset());
    }

    void TestApp::SetImageSpaceSelection(const LLUtils::RectI32& rect)
    {
        fImageSpaceSelection = rect;
        UpdateSelectionRectText();
    }

    LLUtils::RectI32 TestApp::ClientToImageRounded(LLUtils::RectI32 clientRect) const
    {
        return static_cast<LLUtils::RectI32>(ClientToImage(clientRect).Round());
    }

    void TestApp::SaveImageSpaceSelection()
    {
        if (SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(fSelectionRect.GetOperation()))
            SetImageSpaceSelection(ClientToImageRounded(fSelectionRect.GetSelectionRect()));
    }

    void TestApp::LoadImageSpaceSelection()
    {
        if (fImageSpaceSelection.IsEmpty() == false)
        {
            LLUtils::RectI32 r = static_cast<LLUtils::RectI32>(
                ImageToClient(static_cast<LLUtils::RectF64>(fImageSpaceSelection)));
            fSelectionRect.UpdateSelection(r);
            UpdateSelectionRectText();
        }
    }

    void TestApp::CancelSelection()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        if (selectionSizeText != nullptr)
            selectionSizeText->SetVisible(false);

        fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, {0, 0});
        fImageSpaceSelection = decltype(fImageSpaceSelection)::Zero;
    }

    double TestApp::GetMinimumPixelSize()
    {
        return ViewActionController::MinimumPixelSize(fMinImageSize, GetImageSize(ImageSizeType::Transformed));
    }

    void TestApp::QueueResampling()
    {
        if (GetResamplingEnabled() && IsImageOpen() && fDownScalingTechnique == DownscalingTechnique::Software)
        {
            fImageState.SetResample(false);
            fTimerNoActiveZoom.SetInterval(0);
            fTimerNoActiveZoom.SetInterval(fQueueResamplingDelay);
        }
    }

    void TestApp::SetResamplingEnabled(bool enable)
    {
        if (fIsResamplingEnabled != enable)
        {
            fIsResamplingEnabled = enable;
            if (fIsResamplingEnabled == false)
            {
                fTimerNoActiveZoom.SetInterval(0);
                fImageState.SetResample(false);
            }
            else
            {
                QueueResampling();
            }
        }
    }

    bool TestApp::GetResamplingEnabled() const
    {
        return fIsResamplingEnabled;
    }

    void TestApp::SetZoomInternal(double zoomValue, int clientX, int clientY, bool preserveFitToScreenState)
    {
        using namespace LLUtils;

        // Apply zoom limits only if zoom is not bound to the client window
        if (fIsLockFitToScreen == false)
        {
            // We want to keep the image at least the size of 'MinImagePixelsInSmallAxis' pixels in the smallest axis.
            zoomValue = ViewActionController::ResolveZoomValue(zoomValue,
                                                               fIsLockFitToScreen,
                                                               GetMinimumPixelSize(),
                                                               fMaxPixelSize);
        }

        if (zoomValue != fImageState.GetScale().x)
        {
            // Save image selection before view change
            fPreserveImageSpaceSelection.Begin();

            const PointI32 clientZoomPoint =
                ViewActionController::ResolveZoomPoint({clientX, clientY}, GetCanvasCenter());

            PointF64 imageZoomPoint = ClientToImage(clientZoomPoint);
            PointF64 offset = ViewTransformController::ZoomOffset(imageZoomPoint, GetScale(), zoomValue);

            QueueResampling();

            fImageState.SetScale(zoomValue);

            fRefreshOperation.Begin();

            RefreshImage();

            // preserve offset lock (image centering) if zoom is realtive to the center of the image
            SetOffset(GetOffset() + offset,
                      ViewActionController::ShouldPreserveOffsetLockForZoom(clientX, clientY));
            fPreserveImageSpaceSelection.End();

            fRefreshOperation.End();

            /*UpdateCanvasSize();
            UpdateUIZoom();*/

            if (preserveFitToScreenState == false)
                fIsLockFitToScreen = false;
        }
    }

    double TestApp::GetScale() const
    {
        return fImageState.GetScale().x;
    }

    /*void TestApp::UpdateCanvasSize()
    {
        if (fImageState.GetImage(ImageChainStage::Deformed) != nullptr)
        {
            using namespace  LLUtils;
            PointF64 canvasSize = (PointF64)fWindow.GetCanvasSize() / GetScale();
            std::wstringstream ss;
            ss << L"Canvas: "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.x
                << L" X "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.y;
            fWindow.SetStatusBarText(ss.str(), 3, 0);
        }
    }*/

    LLUtils::PointF64 TestApp::GetOffset() const
    {
        return fImageState.GetOffset();
    }

    LLUtils::PointF64 TestApp::ImageToClient(LLUtils::PointF64 imagepos) const
    {
        return ViewTransformController::ImageToClient(imagepos, GetScale(), GetOffset());
    }

    LLUtils::RectF64 TestApp::ImageToClient(LLUtils::RectF64 clientRect) const
    {
        return ViewTransformController::ImageToClient(clientRect, GetScale(), GetOffset());
    }

    LLUtils::PointF64 TestApp::ClientToImage(LLUtils::PointI32 clientPos) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::PointF64>(clientPos), GetScale(), GetOffset());
    }

    LLUtils::RectF64 TestApp::ClientToImage(LLUtils::RectI32 clientRect) const
    {
        return ViewTransformController::ClientToImage(static_cast<LLUtils::RectF64>(clientRect), GetScale(), GetOffset());
    }

    void TestApp::UpdateTexelPos()
    {
        if (fVirtualStatusBar.GetVisible() == true)
        {
            if (fImageState.GetImage(ImageChainStage::Deformed) != nullptr)
            {
                using namespace LLUtils;
                PointF64 storageImageSpace = ClientToImage(fWindow.GetMousePosition());

                std::wstringstream ss;
                ss << L"Texel: " << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6)
                   << storageImageSpace.x << L" X " << std::fixed << std::setprecision(1) << std::setfill(L' ')
                   << std::setw(6) << storageImageSpace.y;
                fVirtualStatusBar.SetText("texelPos", ss.str());

                // fWindow.SetStatusBarText(ss.str(), 2, 0);

                PointF64 storageImageSize = GetImageSize(ImageSizeType::Transformed);

                if (!(storageImageSpace.x < 0 || storageImageSpace.y < 0 || storageImageSpace.x >= storageImageSize.x ||
                      storageImageSpace.y >= storageImageSize.y))
                {
                    std::wstring message = StringUtility::ConvertString<OIVString>(
                        OIVHelper::ParseTexelValue(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(),
                                                   static_cast<LLUtils::PointI32>(storageImageSpace)));
                    OIVString txt = LLUtils::StringUtility::ConvertString<OIVString>(message);
                    fVirtualStatusBar.SetText("texelValue", txt);
                    fVirtualStatusBar.SetOpacity("texelValue", 1.0);
                    fRefreshOperation.Queue();
                }
                else
                {
                    if (fVirtualStatusBar.SetOpacity("texelValue", 0))
                        fRefreshOperation.Queue();
                }
            }
        }
    }
    void TestApp::AutoPlaceImage(bool forceCenter)
    {
        fRefreshOperation.Begin();
        if (ViewActionController::ShouldFitToScreenOnAutoPlace(fIsLockFitToScreen, fIsOffsetLocked))
            FitToClientAreaAndCenter();
        else if (ViewActionController::ShouldCenterOnAutoPlace(fIsLockFitToScreen, fIsOffsetLocked, forceCenter))
            Center();
        fRefreshOperation.End();
    }

    void TestApp::UpdateWindowSize()
    {
        SIZE size = fWindow.GetCanvasSize();

        if (size.cx > 0 && size.cy > 0)  // window might minimized.
        {
            CmdSetClientSizeRequest req{static_cast<uint16_t>(size.cx), static_cast<uint16_t>(size.cy)};

            OIVCommands::ExecuteCommand(CMD_SetClientSize, &req, &NullCommand);
            // UpdateCanvasSize();
            AutoPlaceImage();
            auto point = static_cast<LLUtils::PointI32>(fWindow.GetCanvasSize());
            fVirtualStatusBar.ClientSizeChanged(point);

            EventManager::GetSingleton().SizeChange.Raise(
                EventManager::SizeChangeEventParams{static_cast<int32_t>(size.cx), static_cast<int32_t>(size.cy)});
        }
    }

    LLUtils::PointF64 TestApp::GetCanvasCenter()
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
            auto leftDelta = primaryMonitorCoords.left - boundingArea.left;
            auto topDelta = primaryMonitorCoords.top - boundingArea.top;

            const LLUtils::PointF64 primaryScreenOffset = LLUtils::PointF64(static_cast<point_type>(leftDelta),
                                                                            static_cast<point_type>(topDelta));

            const LLUtils::PointF64 primaryScreenSize = LLUtils::PointF64(
                static_cast<point_type>(primaryMonitorCoords.right - primaryMonitorCoords.left),
                static_cast<point_type>(primaryMonitorCoords.bottom - primaryMonitorCoords.top));

            canvasCenter = primaryScreenOffset + primaryScreenSize / 2.0;
        }
        return canvasCenter;
    }

    void TestApp::Center()
    {
        if (IsImageOpen() == true)
        {
            fRefreshOperation.Begin();
            using namespace LLUtils;
            PointF64 offset =
                ViewTransformController::CenterOffset(GetCanvasCenter(), GetImageSize(ImageSizeType::Visible));
            // Lock offset when centering
            fIsOffsetLocked = true;
            SetOffset(offset, true);
            fRefreshOperation.End();
        }
    }

    LLUtils::PointF64 TestApp::ResolveOffset(const LLUtils::PointF64& point)
    {
        using namespace LLUtils;
        return ViewTransformController::ResolveOffset(point,
                                                      static_cast<PointF64>(fWindow.GetCanvasSize()),
                                                      GetImageSize(ImageSizeType::Visible),
                                                      fImageMargins);
    }

    void TestApp::TransformImage(IMUtil::AxisAlignedRotation relativeRotation, IMUtil::AxisAlignedFlip flip)
    {
        fRefreshOperation.Begin();
        SetResamplingEnabled(false);
        fImageState.Transform(relativeRotation, flip);
        AutoPlaceImage(true);
        RefreshImage();
        fRefreshOperation.End();
        SetResamplingEnabled(true);
    }

    void TestApp::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                          IMCodec::TexelFormat texelFormat)
    {
        std::shared_ptr<OIVRawImage> rawImage = std::make_shared<OIVRawImage>(ImageSource::Clipboard);
        RawBufferParams params;
        params.width = width;
        params.height = height;
        params.rowPitch = rowPitch;
        params.texelFormat = texelFormat;
        params.buffer = buffer;
        // TODO: uncouple vertical flip from 'LoadRaw'
        ResultCode result = rawImage->Load(params,
                                           {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical});

        if (result == RC_Success)
            LoadOivImage(rawImage);
    }

    ClipboardDataType TestApp::PasteFromClipBoard()
    {
        ClipboardDataType clipboardType = ClipboardDataType::None;
        const auto& [formatType, buffer] = fClipboardHelper.GetClipboardData();

        if (formatType == CF_DIB || formatType == CF_DIBV5)
        {
            const tagBITMAPINFO* bitmapInfo = reinterpret_cast<const tagBITMAPINFO*>(buffer.data());
            const BITMAPINFOHEADER* info = &(bitmapInfo->bmiHeader);
            uint32_t rowPitch = LLUtils::Utility::Align<uint32_t>(info->biWidth * (info->biBitCount / CHAR_BIT), 4);

            const std::byte* bitmapBitsconst = reinterpret_cast<const std::byte*>(info) + info->biSize;
            std::byte* bitmapBits = const_cast<std::byte*>(bitmapBitsconst);

            switch (info->biCompression)
            {
                case BI_RGB:
                    break;
                case BI_BITFIELDS:
                    bitmapBits += 3 * sizeof(DWORD);
                    break;
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented,
                                 std::string("Unsupported clipboard bitmap compression type :") +
                                     std::to_string(info->biCompression));
            }

            using namespace IMCodec;
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
            ImageDescriptor& props = imageItem->descriptor;

            imageItem->itemType = ImageItemType::Image;
            props.height = info->biHeight;
            props.width = info->biWidth;
            props.texelFormatStorage = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8
                                                              : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.texelFormatDecompressed = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8
                                                                   : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.rowPitchInBytes = rowPitch;
            const size_t bufferSize = props.rowPitchInBytes * props.height;
            imageItem->data.Allocate(bufferSize);
            imageItem->data.Write(bitmapBits, 0, bufferSize);
            auto image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);

            if (info->biCompression == BI_BITFIELDS)  // no support for alpha channel, convert to BGR
                image = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_B8_G8_R8);

            image = IMUtil::ImageUtil::Transform({IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical},
                                                 image);

            std::shared_ptr<OIVBaseImage> rawImage = std::make_shared<OIVBaseImage>(ImageSource::Clipboard, image);

            LoadOivImage(rawImage);
            clipboardType = ClipboardDataType::Image;
        }

        else if (formatType == CF_UNICODETEXT || formatType == CF_TEXT)
        {
            std::wstring text;
            /*if (isHTMLFormat)
                text = LLUtils::StringUtility::ToWString((char*)clipboardBuffer);
            if (isRTFText)
                text = LLUtils::StringUtility::ToWString((char*)clipboardBuffer);*/
            if (formatType == CF_UNICODETEXT)
                text = (wchar_t*) buffer.data();
            else if (formatType == CF_TEXT)
                text = LLUtils::StringUtility::ToWString((const char*) buffer.data());

            if (text.empty() == false)
            {
                OIVTextImageSharedPtr textImage = std::make_shared<OIVTextImage>(ImageSource::ClipboardText,
                                                                                 fFreeType.get());
                textImage->SetText(text);
                textImage->SetPosition(LLUtils::PointF64::Zero);
                textImage->SetScale(LLUtils::PointF64::One);
                textImage->SetFilterType(OIV_Filter_type::FT_None);
                textImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_MainImage);
                textImage->SetVisible(true);
                textImage->SetOpacity(1.0);

                textImage->SetDPI(fCurrentMonitorProperties.DPIx, fCurrentMonitorProperties.DPIy);
                textImage->SetDPI(fCurrentMonitorProperties.DPIx, fCurrentMonitorProperties.DPIy);
                textImage->SetFontPath(LabelManager::sFontPath);
                textImage->SetFontSize(10);
                textImage->SetOutlineWidth(0);
                textImage->SetTextColor({48, 48, 48, 255});
                textImage->SetUseMetaText(false);
                // text->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
                textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
                // textImage->Create();
                textImage->Create();
                // textImage->GetImage();
                LoadOivImage(textImage);
                clipboardType = ClipboardDataType::Text;
            }
        }
        return clipboardType;
    }

    bool TestApp::SetClipboardImage(IMCodec::ImageSharedPtr image)
    {
        auto clipboardCompatibleImage = IMUtil::ImageUtil::ConvertImageWithNormalization(
            image, IMCodec::TexelFormat::I_B8_G8_R8_A8, false);
        if (clipboardCompatibleImage != nullptr)
        {
            uint32_t width = clipboardCompatibleImage->GetWidth();
            uint32_t height = clipboardCompatibleImage->GetHeight();
            uint8_t bpp = clipboardCompatibleImage->GetBitsPerTexel();
            auto dibBUffer = LLUtils::PlatformUtility::CreateDIB<1>(width, height, bpp,
                                                                    clipboardCompatibleImage->GetRowPitchInBytes(),
                                                                    clipboardCompatibleImage->GetBuffer());
            auto result = fClipboardHelper.SetClipboardData(CF_DIB, dibBUffer);

            if (result == ::Win32::ClipboardResult::Success)
            {
                auto dibV5BUffer = LLUtils::PlatformUtility::CreateDIB<5>(
                    width, height, bpp, clipboardCompatibleImage->GetRowPitchInBytes(),
                    clipboardCompatibleImage->GetBuffer());
                result = fClipboardHelper.SetClipboardData(CF_DIBV5, dibV5BUffer);
            }

            if (result == ::Win32::ClipboardResult::Success)
                return true;
        }
        return false;
    }

    OperationResult TestApp::CopyVisibleToClipBoard()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(),
            fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result = OperationResult::UnkownError;
            LLUtils::RectI32 imageSpaceSelection = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped = IMUtil::ImageUtil::CropImage(
                fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(), imageSpaceSelection);

            if (cropped != nullptr)
            {
                // 2. Flip the image vertically and convert it to BGRA for the clipboard.
                auto flipped = IMUtil::ImageUtil::Transform(
                    {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical}, cropped);
                if (flipped != nullptr && SetClipboardImage(flipped))
                    result = OperationResult::Success;
            }
        }
        return result;
    }

    OperationResult TestApp::CropVisibleImage()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(),
            fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result = OperationResult::UnkownError;
            LLUtils::RectI32 imageRectInt = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped = IMUtil::ImageUtil::CropImage(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(),
                                                        imageRectInt);

            if (cropped != nullptr)
            {
                auto oivCropped = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, cropped);
                LoadOivImage(oivCropped);
                CancelSelection();
                result = OperationResult::Success;
            }
        }
        return result;
    }

    OperationResult TestApp::CutSelectedArea()
    {
        // Please note that currently this function works on the rasterized image, a more general solution is needed
        // to work on a previous stage image.
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(),
            fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result = OperationResult::UnkownError;
            auto rasterized = fImageState.GetImage(ImageChainStage::Rasterized)->GetImage();
            LLUtils::RectI32 subImageRect = ClientToImageRounded(fSelectionRect.GetSelectionRect());

            const LLUtils::RectI32 imageRect = {{0, 0},
                                                {static_cast<int32_t>(rasterized->GetWidth()),
                                                 static_cast<int32_t>(rasterized->GetHeight())}};

            subImageRect = subImageRect.Intersection(imageRect);

            if (subImageRect.IsEmpty() == false)
            {
                SetClipboardImage(IMUtil::ImageUtil::GetSubImage(rasterized, subImageRect));
                auto& texelInfo = IMCodec::GetTexelInfo(
                    fImageState.GetImage(ImageChainStage::Rasterized)->GetImage()->GetOriginalTexelFormat());
                bool hasOpacityChannel = false;
                for (auto& channel : texelInfo.channles)
                    if (channel.semantic == IMCodec::ChannelSemantic::Opacity)
                    {
                        hasOpacityChannel = true;
                        break;
                    }

                const auto fillColor = hasOpacityChannel ? LLUtils::Color(0, 0, 0, 0) : LLUtils::Color(0, 0, 0, 255);
                auto colorFilled = IMUtil::ImageUtil::FillColor(
                    fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(), subImageRect, fillColor);

                if (colorFilled != nullptr)
                {
                    auto oivColorFilled = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, colorFilled);
                    auto lastState = fResetTransformationMode;
                    fResetTransformationMode = ResetTransformationMode::DoNothing;
                    LoadOivImage(oivColorFilled);
                    fResetTransformationMode = lastState;
                    CancelSelection();
                    result = OperationResult::Success;
                }
            }
        }
        return result;
    }

    void TestApp::AfterFirstFrameDisplayed()
    {
        PostInitOperations();
    }

    LRESULT TestApp::ClientWindwMessage(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);
        if (evnt == nullptr)
            return 0;

        const WinMessage& message = evnt->message;

        LRESULT retValue = 0;
        switch (message.message)
        {
            case WM_SIZE:
                fRefreshOperation.Begin();
                UpdateWindowSize();
                fRefreshOperation.End();
                break;
        }
        return retValue;
    }

    void TestApp::SetTopMostUserMesage()
    {
        SetUserMessage(ViewerPresentationPolicy::FormatTopMostMessage(fTopMostCounter),
                       static_cast<GroupID>(UserMessageGroups::WindowOnTop),
                       MessageFlags::Interchangeable | MessageFlags::ManualRemove);
    }

    bool TestApp::GetAppActive() const
    {
        return fIsActive;
    }

    void TestApp::SetAppActive(bool active)
    {
        if (active != fIsActive)
        {
            fIsActive = active;
            if (fIsActive == true && fFileReloadPolicy.HasPendingReloadFor(GetOpenedFileName()))
            {
                PerformReloadFile(GetOpenedFileName());
            }
            else
            {
                UpdateTitle();
            }
        }
    }

    void TestApp::ProcessTopMost()
    {
        if (fTopMostCounter > 0)
        {
            fTopMostCounter--;

            if (fTopMostCounter == 0)
            {
                fTimerTopMostRetention.SetInterval(0);
                fWindow.SetAlwaysOnTop(false);
                fMessageManager->RemoveGroup(static_cast<GroupID>(UserMessageGroups::WindowOnTop));
            }
            else
                SetTopMostUserMesage();
        }
    }

    void TestApp::PerformReloadFile(const std::wstring& requestedFile)
    {
        HandleReloadAction(fFileReloadPolicy.OnPendingReloadRequested(requestedFile), requestedFile);
    }

    void TestApp::HandleReloadAction(ReloadAction action, const std::wstring& requestedFile)
    {
        if (action == ReloadAction::AskUser)
        {
            using namespace std::string_literals;
            int mbResult = MessageBox(fWindow.GetHandle(), (L"Reload the file: "s + requestedFile).c_str(),
                                      L"File is changed outside of OIV", MB_YESNO);
            action = fFileReloadPolicy.ConfirmReload(mbResult == IDYES);
        }

        if (action == ReloadAction::RequestNow && fFileSessionController != nullptr)
            fFileSessionController->RequestCurrentFileReload();
    }

    void TestApp::ProcessCurrentFileChanged()
    {
        HandleReloadAction(fFileReloadPolicy.OnCurrentFileChanged(GetOpenedFileName(), GetAppActive()),
                           GetOpenedFileName());
    }

    bool TestApp::HandleWinMessageEvent(const ::Win32::EventWinMessage* evnt)
    {
        bool handled = false;

        const ::Win32::WinMessage& uMsg = evnt->message;
        switch (uMsg.message)
        {
            case WM_SHOWWINDOW:
                if (fIsFirstFrameDisplayed == false && uMsg.wParam == TRUE)
                {
                    PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_FIRST_FRAME_DISPLAYED, 0, 0);
                    fIsFirstFrameDisplayed = true;
                }
                break;
            case Win32::UserMessage::PRIVATE_WN_FIRST_FRAME_DISPLAYED:
                AfterFirstFrameDisplayed();
                break;

            case Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL:
                fAutoScroll->PerformAutoScroll();
                break;
            case Win32::UserMessage::PRIVATE_WM_NOTIFY_FILE_CHANGED:
                OnFileChangedImpl(reinterpret_cast<FileWatcher::FileChangedEventArgs*>(uMsg.wParam));
                break;

            case WM_COPYDATA:
            {
                COPYDATASTRUCT* cds = (COPYDATASTRUCT*) uMsg.lParam;
                if (uMsg.wParam == ::OIV::Win32::UserMessage::PRIVATE_WM_LOAD_FILE_EXTERNALLY)
                {
                    wchar_t* fileToLoad = reinterpret_cast<wchar_t*>(cds->lpData);
                    LoadFile(fileToLoad, IMCodec::PluginTraverseMode::NoTraverse);
                    fWindow.SetVisible(true);
                }
            }
            break;

            case WM_SYSKEYUP:
            case WM_KEYUP:
            {
                using namespace LInput;
                KeyCombination keyCombination = KeyCombination::FromVirtualKey(
                    static_cast<uint32_t>(evnt->message.wParam), static_cast<uint32_t>(evnt->message.lParam));

                bool isAltup = (keyCombination.keydata().keycode == KeyCode::LALT ||
                                keyCombination.keydata().keycode == KeyCode::RIGHTALT ||
                                keyCombination.keydata().keycode == KeyCode::RALT);

                if (isAltup)
                    fDoubleTap.SetState(false);
            }
            break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                handled = handleKeyInput(evnt);
                break;

            case WM_MOUSEMOVE:
                UpdateTexelPos();
                break;
            case WM_CLOSE:
                CloseApplication(false);
                break;
            case WM_ACTIVATE:
                SetAppActive(uMsg.wParam != WA_INACTIVE);
                break;
        }

        return handled;
    }

    void TestApp::CloseApplication(bool closeToTray)
    {
        HANDLE mutex = CreateMutex(
            NULL, FALSE, (LLUtils::native_string_type(Globals::ProgramGuid) + LLUTILS_TEXT("_CLOSEAPP")).c_str());
        if (mutex == nullptr)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be created.");
        }

        const DWORD result = WaitForSingleObject(mutex,      // handle to mutex
                                                 INFINITE);  // no time-out interval

        switch (result)
        {
            case WAIT_OBJECT_0:

                fWindow.SetVisible(false);

                if (closeToTray == false || FindTrayBarWindow() != nullptr)
                {
                    fWindow.Destroy();
                }
                else
                {
                    fWindow.SetIsTrayWindow(true);
                }

                break;
            default:
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex ownership cannot be acquired.");
        }

        if (!ReleaseMutex(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be released.");

        if (CloseHandle(mutex) == FALSE)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be closed.");
    }

    bool TestApp::LoadFileOrFolder(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode)
    {
        const auto clientSize = fWindow.GetClientSize();
        return ProcessImageLoadResult(fImageLoadController->LoadFileOrFolder(
            filePath,
            traverseMode,
            ImageLoadContext{static_cast<int>(clientSize.cx), static_cast<int>(clientSize.cy)}));
    }

    bool TestApp::HandleFileDragDropEvent(const ::Win32::EventDdragDropFile* event_ddrag_drop_file)
    {
        std::wstring normalizedPath =
            std::filesystem::path(event_ddrag_drop_file->fileName).lexically_normal().wstring();
        if (LoadFileOrFolder(normalizedPath,
                             IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
        {
            fWindow.SetForground();
            return true;
        }

        return false;
    }

    bool TestApp::ExecutePredefinedCommand(std::string command)
    {
        CommandManager::CommandRequest commandRequest = fCommandManager.GetCommandRequestGroup(command);
        return commandRequest.commandName.empty() == false && ExecuteCommand(commandRequest);
    }

    bool TestApp::HandleClientWindowMessages(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
        {
            return ClientWindwMessage(evnt);
        }
        return false;
    }

    bool TestApp::HandleMessages(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
            return HandleWinMessageEvent(evnt);

        const EventDdragDropFile* dragDropEvent = dynamic_cast<const EventDdragDropFile*>(evnt1);

        if (dragDropEvent != nullptr)
            return HandleFileDragDropEvent(dragDropEvent);

        return false;
    }

    void TestApp::SetUserMessage(const std::wstring& message, GroupID groupID, MessageFlags groupFlags)
    {
        fMessageManager->SetUserMessage(groupID, groupFlags, message);
    }

    bool TestApp::ExecuteCommandInternal(const CommandRequestIntenal& requestInternal)
    {
        CommandManager::CommandRequest request{"", requestInternal.commandName,
                                               CommandManager::CommandArgs::FromString(requestInternal.args)};
        return ExecuteCommand(request);
    }

    bool TestApp::ExecuteCommand(const CommandManager::CommandRequest& request)
    {
        CommandManager::CommandResult res;
        if (fCommandManager.ExecuteCommand(request, res))
        {
            if (res.resValue.empty() == false)
                SetUserMessage(res.resValue);
            return true;
        }
        return false;
    }

    void TestApp::CountColorsAsync()
    {
        // Ensure shared tr refcount doesn't get to zero
        //  by assiging it to a private memeber field.

        auto openedImage = fImageState.GetImage(ImageChainStage::SourceImage);

        // Count colors ONLY if non initialized, meaning it's the first time of trying to count colors
        if (openedImage->GetNumUniqueColors() == UniqueColorsUninitialized)
        {
            if (fIsColorThreadRunning == false)
            {
                fIsColorThreadRunning = true;
                if (fCountingColorsThread.joinable())
                    fCountingColorsThread.join();

                fCountingImageColor = openedImage;
                fCountingColorsThread = std::thread(
                    [&](OIVBaseImageSharedPtr image) -> void
                    {
                        int64_t uniqueValues = PixelHelper::CountUniqueValues(image->GetImage());
                        fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                               InterThreadMessages::CountColors),
                                           CountColorsData{image.get(), uniqueValues});
                    },
                    fCountingImageColor);
            }
        }
    }

    void TestApp::ShowImageInfo()
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

    void TestApp::ShowWelcomeMessage()
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

    void TestApp::UnloadWelcomeMessage()
    {
        fLabelManager.Remove("welcomeMessage");
    }

    void TestApp::SetDownScalingTechnique(DownscalingTechnique technique)
    {
        if (technique != fDownScalingTechnique)
        {
            fDownScalingTechnique = technique;
            switch (fDownScalingTechnique)
            {
                case DownscalingTechnique::None:
                    fImageState.SetResample(false);
                    break;
                case DownscalingTechnique::HardwareMipmaps:
                    fImageState.SetResample(false);
                    break;
                case DownscalingTechnique::Software:
                    fImageState.SetResample(true);
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            if (IsImageOpen() == true)
            {
                fRefreshOperation.Begin();
                RefreshImage();
                UpdateRenderViewParams();
                fRefreshOperation.End();
            }
        }
    }

    // IFileListWatcher

    FileListStringType TestApp::GetActiveFileName()
    {
        return GetOpenedFileName();
    }

}  // namespace OIV
