#include <iomanip>
#include <LLUtils/StringUtility.h>
#include <string>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include "OIVCommands.h"
#include "PlatformFileDialog.h"

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
    void ViewerApplication::CMD_Zoom(const CommandManager::CommandRequest& request,
                                     CommandManager::CommandResult& result)
    {
        if (IsImageOpen())
        {
            const ZoomCommand command = ViewCommandPolicy::ParseZoom(request.args);
            ZoomInternal(command.amount, command.centerX, command.centerY);
            result.resValue = ViewCommandPolicy::FormatZoomResult(GetScale());
        }
    }

    void ViewerApplication::CMD_ViewState(const CommandManager::CommandRequest& request,
                                          CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        string type = request.args.GetArgValue("type");

        bool fullscreenModeChanged = false;
        bool filterTypeChanged     = false;

        if (type == "toggleBorders")
        {
            ToggleBorders();
            result.resValue = LLUtils::native_string_type(LLUTILS_TEXT("Borders ")) +
                              (fShowBorders == true ? LLUTILS_TEXT("On") : LLUTILS_TEXT("Off"));
        }
        else if (type == "quit")
        {
            string op        = request.args.GetArgValue("op");
            bool closeToTray = op == "closetotray";
            CloseApplication(closeToTray);
        }
        else if (type == "grid")
        {
            ToggleGrid();
            result.resValue = LLUTILS_TEXT("Grid ");
            result.resValue += fIsGridEnabled == true ? LLUTILS_TEXT("on") : LLUTILS_TEXT("off");
        }
        else if (type == "slideShow")
        {
            SetSlideShowEnabled(!GetSlideShowEnabled());
            result.resValue = LLUTILS_TEXT("Slideshow ");
            result.resValue += fTimerSlideShow.GetInterval() > 0 ? LLUTILS_TEXT("on") : LLUTILS_TEXT("off");
        }
        else if (type == "toggleNormalization")
        {
            // Change normalization mode
            fImageState.SetUseRainbowNormalization(!fImageState.GetUseRainbowNormalization());
            RefreshImage();
            result.resValue = fImageState.GetUseRainbowNormalization() ? LLUTILS_TEXT("Rainbow normalization")
                                                                       : LLUTILS_TEXT("Grayscale normalization");
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
                                  ? LLUTILS_TEXT("Don't auto reset image state")
                                  : LLUTILS_TEXT("Auto reset image state");
        }
        else if (type == "toggletransparencymode")
        {
            fTransparencyMode = static_cast<OIV_PROP_TransparencyMode>(
                (fTransparencyMode + 1) % static_cast<int>(OIV_PROP_TransparencyMode::TM_Count));
            UpdateRenderViewParams();

            LLUtils::native_string_type userMessage = LLUTILS_TEXT("Transparency: ");
            LLUtils::native_string_type transparencyMode;

            switch (fTransparencyMode)
            {
                case OIV_PROP_TransparencyMode::TM_Light:
                    transparencyMode = LLUTILS_TEXT("Light");
                    break;
                case OIV_PROP_TransparencyMode::TM_Medium:
                    transparencyMode = LLUTILS_TEXT("Medium(default)");
                    break;
                case OIV_PROP_TransparencyMode::TM_Dark:
                    transparencyMode = LLUTILS_TEXT("Dark");
                    break;
                case OIV_PROP_TransparencyMode::TM_Darker:
                    transparencyMode = LLUTILS_TEXT("Darker");
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            result.resValue = userMessage + LLUTILS_TEXT("<textcolor=#7672ff>") + transparencyMode;
        }
        else if (type == "toggledownsamplingtechnique")
        {
            DownscalingTechnique technique = GetNextEnumValue(fDownScalingTechnique);

            LLUtils::native_string_type downscaleTechnique;

            switch (technique)
            {
                case DownscalingTechnique::None:
                    downscaleTechnique = LLUTILS_TEXT("No downsamling");
                    break;
                case DownscalingTechnique::HardwareMipmaps:
                    downscaleTechnique = LLUTILS_TEXT("Hardware mipmaps");
                    break;
                case DownscalingTechnique::Software:
                    downscaleTechnique = LLUTILS_TEXT("Box filter");
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }
            result.resValue = DefaultTextKeyColorTag + LLUTILS_TEXT("Downscaling technique: ") +
                              DefaultTextValueColorTag + downscaleTechnique;

            SetDownScalingTechnique(technique);
        }
        else if (type == "toggleStatusBar")
        {
            fVirtualStatusBar.SetVisible(!fVirtualStatusBar.GetVisible());
            fRefreshOperation.Queue();
        }

        if (fullscreenModeChanged == true)
        {
            switch (fWindow.GetWindow().GetWindowMode())
            {
                case LWS::WindowMode::FullscreenAllMonitors:
                    result.resValue = LLUTILS_TEXT("Multi full screen");
                    break;
                case LWS::WindowMode::Fullscreen:
                    result.resValue = LLUTILS_TEXT("Full screen");
                    break;
                case LWS::WindowMode::Windowed:
                    result.resValue = LLUTILS_TEXT("Windowed");
                    break;
            }
        }

        if (filterTypeChanged == true)
        {
            switch (GetFilterType())
            {
                case FT_None:
                    result.resValue = LLUTILS_TEXT("No filtering");
                    break;
                case FT_Linear:
                    result.resValue = LLUTILS_TEXT("Linear filtering");
                    break;
                case FT_Lanczos3:
                    result.resValue = LLUTILS_TEXT("Lanczos3 filtering");
                    break;
                case FT_Count:
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                    break;
            }
        }
    }

    void ViewerApplication::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request,
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

            text         = fLabelManager.GetOrCreateTextLabel("keyBindings");
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

    void ViewerApplication::CMD_ShowSystemInfo([[maybe_unused]] const CommandManager::CommandRequest& request,
                                               [[maybe_unused]] CommandManager::CommandResult& result)
    {
        OIVTextImage* text = fLabelManager.GetTextLabel("systemInfo");
        if (text != nullptr)
        {
            fLabelManager.Remove("systemInfo");
            fRefreshOperation.Queue();
            return;
        }

        text = fLabelManager.GetOrCreateTextLabel("systemInfo");

#if defined(NDEBUG)
        constexpr std::string_view buildType = "Release";
#else
        constexpr std::string_view buildType = "Debug";
#endif

        auto message = MessageHelper::CreateSystemInfoMessage({
            .appName    = "OpenImageViewer",
            .appVersion = OIV::FormatFullVersion(OIV::CurrentVersion),
            .gitHash    = OIV_GIT_SHORT_HASH,
            .buildType  = buildType,
            .renderer   = OIV::ApiGlobal::sPictureRenderer->GetRenderer(),
        });

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

    void ViewerApplication::CMD_OpenFile([[maybe_unused]] const CommandManager::CommandRequest& request,
                                         [[maybe_unused]] CommandManager::CommandResult& response)
    {
        std::string cmd = request.args.GetArgValue("cmd");
        if (cmd == "savefile")
        {
            LLUtils::native_string_type saveFilePath;
            LLUtils::native_string_type defaultFileName;
            if (IsImageOpen())
            {
                auto openedImage = fImageState.GetOpenedImage();
                auto imageSource = openedImage->GetImageSource();
                switch (imageSource)
                {
                    case ImageSource::ClipboardText:
                        defaultFileName = LLUTILS_TEXT("text");
                        break;
                    case ImageSource::File:
                    {
                        std::filesystem::path p = std::dynamic_pointer_cast<OIVFileImage>(openedImage)->GetFileName();
                        defaultFileName         = p.stem();
                    }
                    break;
                    case ImageSource::Clipboard:
                        defaultFileName = LLUTILS_TEXT("clipboard");
                        break;
                    default:
                        defaultFileName = LLUTILS_TEXT("image");
                        break;
                }

                auto result = PlatformFileDialog::Show(LWS::FileDialogType::SaveFile, fSaveComDlgFilters.GetFilters(),
                                                       LLUTILS_TEXT("Save an image"), fWindow.GetWindow(),
                                                       LLUTILS_TEXT("*.") + fDefaultSaveFileExtension,
                                                       fDefaultSaveFileFormatIndex, defaultFileName, saveFilePath);

                if (result == LWS::FileDialogResult::Success)
                {
                    LLUtils::native_string_type extension = LLUtils::StringUtility::ToLower(
                        std::filesystem::path(saveFilePath).extension().native());
                    LLUtils::native_string_view sv(extension);

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
            LLUtils::native_string_type openFilePath;
            auto result = PlatformFileDialog::Show(LWS::FileDialogType::OpenFile, fOpenComDlgFilters.GetFilters(),
                                                   LLUTILS_TEXT("Open image"), fWindow.GetWindow(), {}, 0, {},
                                                   openFilePath);

            if (result == LWS::FileDialogResult::Success)
                LoadFile(openFilePath, IMCodec::PluginTraverseMode::NoTraverse);
        }
    }

    void ViewerApplication::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request,
                                                     CommandManager::CommandResult& response)
    {
        const AxisAlignedTransformCommand command = ImageTransformCommandPolicy::ParseAxisAlignedTransform(
            request.args);

        if (command.HasTransform())
        {
            TransformImage(command.rotation, command.flip);
            response.resValue = ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                fImageState.GetAxisAlignedRotation(), fImageState.GetAxisAlignedFlip());
        }
    }

    void ViewerApplication::CMD_ToggleColorCorrection([[maybe_unused]] const CommandManager::CommandRequest& request,
                                                      CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        if (ToggleColorCorrection())
            result.resValue = LLUTILS_TEXT("Reset color correction to previous");
        else
            result.resValue = LLUTILS_TEXT("Reset color correction to default");
    }

    void ViewerApplication::CMD_SetWindowSize(const CommandManager::CommandRequest& request,
                                              CommandManager::CommandResult& result)
    {
        const WindowSizeDecision decision = GetWindowSizeDecision(request.args);

        switch (decision.mode)
        {
            case WindowSizeMode::Fullscreen:
                std::ignore = fWindow.GetWindow().SetWindowMode(LWS::WindowMode::Fullscreen);
                break;
            case WindowSizeMode::MultiFullscreen:
                std::ignore = fWindow.GetWindow().SetWindowMode(LWS::WindowMode::FullscreenAllMonitors);
                break;
            case WindowSizeMode::Maximized:
                // Use native maximization for "Window size entire screen" to preserve the normal restore placement.
                // Resizing to the monitor bounds would also fill the usable area, but overwrite that placement
                // and lose the expected maximize/restore behavior.
                std::ignore = fWindow.GetWindow().RequestMaximize();
                break;
            case WindowSizeMode::Windowed:
                std::ignore = fWindow.GetWindow().SetWindowMode(LWS::WindowMode::Windowed);
                std::ignore = fWindow.GetWindow().RequestShowState(LWS::WindowShowState::Restored);

                std::ignore = fWindow.GetWindow().SetPlacement(
                    {.position   = fPlatform.Supports(LWS::PlatformFeature::AbsoluteWindowPosition).value_or(false)
                                       ? std::optional(decision.position)
                                       : std::nullopt,
                     .clientSize = {decision.size.x, decision.size.y}});
                break;
            case WindowSizeMode::None:
                break;
        }

        result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
    }

    void ViewerApplication::CMD_SortFiles(const CommandManager::CommandRequest& request,
                                          CommandManager::CommandResult& result)
    {
        const SortCommandDecision decision = SortCommandPolicy::Decide(request.args, fFileSorter.GetSortType());

        if (decision.valid)
        {
            if (decision.reverseDirection)
                fFileSorter.SetActiveSortDirection(SortCommandPolicy::Reverse(fFileSorter.GetActiveSortDirection()));
            else
                fFileSorter.SetSortType(decision.sortType);
        }

        SortFolderFileList();
        result.resValue = SortCommandPolicy::FormatSortResult(request.displayName,
                                                              fFileSorter.GetActiveSortDirection());
    }

    void ViewerApplication::CMD_Sequencer(const CommandManager::CommandRequest& request,
                                          CommandManager::CommandResult& result)
    {
        auto openedImage = fImageState.GetOpenedImage();
        if (openedImage != nullptr &&
            openedImage->GetImage()->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame)
        {
            if (SequencerPolicy::IsChangeSpeedCommand(request.args))
            {
                fCurrentSequencerSpeed = SequencerPolicy::ApplySpeedChange(
                    fCurrentSequencerSpeed, SequencerPolicy::ParseSpeedChangePercent(request.args));
                result.resValue = SequencerPolicy::FormatSpeed(fCurrentSequencerSpeed);
            }
        }
    }

    void ViewerApplication::CMD_DeleteFile(const CommandManager::CommandRequest& request,
                                           CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        string type = request.args.GetArgValue("type");

        if (type == "recyclebin")
            DeleteOpenedFile(false);
        else if (type == "permanently")
            DeleteOpenedFile(true);

        result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
    }

    void ViewerApplication::CMD_ColorCorrection(const CommandManager::CommandRequest& request,
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

        *value          = ColorCorrectionCommandPolicy::Apply(*value, command.operation, command.value);
        result.resValue = ColorCorrectionCommandPolicy::FormatResult(command, *value);
        UpdateExposure();
    }

    void ViewerApplication::CMD_Pan(const CommandManager::CommandRequest& request,
                                    CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_CopyToClipboard(const CommandManager::CommandRequest& request,
                                                CommandManager::CommandResult& result)
    {
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");

        if (cmd == "fileName")
        {
            if (IsOpenedImageIsAFile())
            {
                fClipboardHelper.SetClipboardText(fWindow.GetWindow(), GetOpenedFileName().c_str());
                result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
            }
        }
        else if (cmd == "selectedArea")
        {
            OperationResult res = CopyVisibleToClipBoard();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(
                    LLUTILS_TEXT("Cannot copy to clipboard"), res);
            else
                result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
        }
        else if (cmd == "cut")
        {
            OperationResult res = CutSelectedArea();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(
                    LLUTILS_TEXT("Cannot cut selected area"), res);
            else
                result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
        }
    }

    void ViewerApplication::CMD_PasteFromClipboard([[maybe_unused]] const CommandManager::CommandRequest& request,
                                                   CommandManager::CommandResult& result)
    {
        switch (PasteFromClipBoard())
        {
            case ClipboardDataType::None:
                result.resValue = LLUTILS_TEXT("Nothing usable in clipboard");
                break;
            case ClipboardDataType::Image:
                result.resValue = LLUTILS_TEXT("Paste image from clipboard");
                break;
            case ClipboardDataType::Text:
                result.resValue = LLUTILS_TEXT("Paste text from clipboard");
                break;
        }
    }

    void ViewerApplication::CMD_ImageManipulation(const CommandManager::CommandRequest& request,
                                                  CommandManager::CommandResult& result)
    {
        using namespace std;
        const string cmd = request.args.GetArgValue("cmd");
        if (cmd == "cropSelectedArea")
        {
            OperationResult res = CropVisibleImage();
            if (res != OperationResult::Success)
                result.resValue = ViewerPresentationPolicy::FormatFailedOperation(
                    LLUTILS_TEXT("Cannot crop selected area"), res);
            else
                result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
        }

        else if (cmd == "selectAll")
        {
            if (fImageState.GetOpenedImage() != nullptr)
            {
                result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
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
                result.resValue = LLUTILS_TEXT("No image loaded");
            }
        }
    }

    void ViewerApplication::CMD_Placement(const CommandManager::CommandRequest& request,
                                          CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_Navigate(const CommandManager::CommandRequest& request,
                                         [[maybe_unused]] CommandManager::CommandResult& result)
    {
        const NavigationCommand command = ViewCommandPolicy::ParseNavigation(request.args);
        if (command.subImage)
        {
            auto& imageList        = fWindow.GetImageControl().GetImageList();
            const auto numElements = imageList.GetNumberOfElements();
            if (numElements > 0)
            {
                imageList.SetSelected(static_cast<int>(
                    ViewCommandPolicy::NextSubImageIndex(imageList.GetSelected(), command.amount, numElements)));
            }
        }
        else
        {
            JumpFiles(command.amount);
        }
    }

    void ViewerApplication::CMD_Shell(const CommandManager::CommandRequest& request,
                                      CommandManager::CommandResult& result)
    {
        result.resValue = ShellCommandHandler::Execute(request, GetOpenedFileName(), fImageState.GetOpenedImage());
    }

    template <typename T>
    LLUtils::native_string_type IntToHex(T val)
    {
        LLUtils::native_stringstream ss;
        ss << std::setfill(LLUTILS_TEXT("0")[0]) << std::setw(sizeof(T) * 2) << std::hex << val;

        return ss.str();
    }

    void ViewerApplication::AddCommandsAndKeyBindings()
    {
        using namespace std;
        using namespace placeholders;

        CommandRegistry::AddConfiguredCommands(fCommandController.GetCommandManager());
        AddPlatformKeyBindings();
        fCommandController.AddCommandCallbacks(
            {{"cmd_color_correction", std::bind(&ViewerApplication::CMD_ColorCorrection, this, _1, _2)},
             {"cmd_view_state", std::bind(&ViewerApplication::CMD_ViewState, this, _1, _2)},
             {"cmd_toggle_correction", std::bind(&ViewerApplication::CMD_ToggleColorCorrection, this, _1, _2)},
             {"cmd_toggle_keybindings", std::bind(&ViewerApplication::CMD_ToggleKeyBindings, this, _1, _2)},
             {"cmd_show_system_info", std::bind(&ViewerApplication::CMD_ShowSystemInfo, this, _1, _2)},
             {"cmd_axis_aligned_transform", std::bind(&ViewerApplication::CMD_AxisAlignedTransform, this, _1, _2)},
             {"cmd_open_file", std::bind(&ViewerApplication::CMD_OpenFile, this, _1, _2)},
             {"cmd_zoom", std::bind(&ViewerApplication::CMD_Zoom, this, _1, _2)},
             {"cmd_pan", std::bind(&ViewerApplication::CMD_Pan, this, _1, _2)},
             {"cmd_placement", std::bind(&ViewerApplication::CMD_Placement, this, _1, _2)},
             {"cmd_copyToClipboard", std::bind(&ViewerApplication::CMD_CopyToClipboard, this, _1, _2)},
             {"cmd_pasteFromClipboard", std::bind(&ViewerApplication::CMD_PasteFromClipboard, this, _1, _2)},
             {"cmd_imageManipulation", std::bind(&ViewerApplication::CMD_ImageManipulation, this, _1, _2)},
             {"cmd_navigate", std::bind(&ViewerApplication::CMD_Navigate, this, _1, _2)},
             {"cmd_shell", std::bind(&ViewerApplication::CMD_Shell, this, _1, _2)},
             {"cmd_delete_file", std::bind(&ViewerApplication::CMD_DeleteFile, this, _1, _2)},
             {"cmd_set_window_size", std::bind(&ViewerApplication::CMD_SetWindowSize, this, _1, _2)},
             {"cmd_sort_files", std::bind(&ViewerApplication::CMD_SortFiles, this, _1, _2)},
             {"cmd_sequencer", std::bind(&ViewerApplication::CMD_Sequencer, this, _1, _2)}});
    }

    bool ViewerApplication::ExecutePredefinedCommand(std::string command)
    {
        return fCommandController.ExecutePredefinedCommand(command);
    }

    bool ViewerApplication::ExecuteCommandInternal(const CommandRequestIntenal& requestInternal)
    {
        return fCommandController.ExecuteCommandInternal(requestInternal.commandName, requestInternal.args);
    }

    bool ViewerApplication::ExecuteCommand(const CommandManager::CommandRequest& request)
    {
        return fCommandController.ExecuteCommand(request);
    }
}  // namespace OIV
