#include <iomanip>
#include <LLUtils/StringUtility.h>
#include <string>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "Viewer.h"

#include <Version.h>

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

#include <LWS/FileDialog.hpp>

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
    void Viewer::CMD_Zoom(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        if (IsImageOpen())
        {
            const ZoomCommand command = ViewCommandPolicy::ParseZoom(request.args);
            ZoomInternal(command.steps, command.centerX, command.centerY);
            result.resValue = ViewCommandPolicy::FormatZoomResult(GetScale());
        }
    }

    void Viewer::CMD_ViewState(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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
            result.resValue += fSession.fIsGridEnabled == true ? LLUTILS_TEXT("on") : LLUTILS_TEXT("off");
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
            fSession.fImageState.SetUseRainbowNormalization(!fSession.fImageState.GetUseRainbowNormalization());
            RefreshImage();
            result.resValue = fSession.fImageState.GetUseRainbowNormalization()
                                  ? LLUTILS_TEXT("Rainbow normalization")
                                  : LLUTILS_TEXT("Grayscale normalization");
        }
        else if (type == "imageFilterUp")
        {
            if (fSession.fImageState.GetVisibleImage() != nullptr)
            {
                SetFilterLevel(static_cast<OIV::FilterType>(static_cast<int>(GetFilterType()) + 1));
                filterTypeChanged = true;
            }
        }
        else if (type == "imageFilterDown")
        {
            if (fSession.fImageState.GetVisibleImage() != nullptr)
            {
                SetFilterLevel(static_cast<OIV::FilterType>(static_cast<int>(GetFilterType()) - 1));
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
            fSession.fResetTransformationMode = static_cast<ResetTransformationMode>(
                (static_cast<int>(fSession.fResetTransformationMode) + 1) %
                static_cast<int>(ResetTransformationMode::Count));
            result.resValue = fSession.fResetTransformationMode == ResetTransformationMode::DoNothing
                                  ? LLUTILS_TEXT("Don't auto reset image state")
                                  : LLUTILS_TEXT("Auto reset image state");
        }
        else if (type == "toggletransparencymode")
        {
            fSession.fTransparencyMode = static_cast<OIV::TransparencyShade>(
                (static_cast<int>(fSession.fTransparencyMode) + 1) % static_cast<int>(OIV::TransparencyShade::Count));
            UpdateRenderViewParams();

            LLUtils::native_string_type userMessage = LLUTILS_TEXT("Transparency: ");
            LLUtils::native_string_type transparencyMode;

            switch (fSession.fTransparencyMode)
            {
                case OIV::TransparencyShade::Light:
                    transparencyMode = LLUTILS_TEXT("Light");
                    break;
                case OIV::TransparencyShade::Medium:
                    transparencyMode = LLUTILS_TEXT("Medium(default)");
                    break;
                case OIV::TransparencyShade::Dark:
                    transparencyMode = LLUTILS_TEXT("Dark");
                    break;
                case OIV::TransparencyShade::Darker:
                    transparencyMode = LLUTILS_TEXT("Darker");
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            result.resValue = userMessage + LLUTILS_TEXT("<textcolor=#7672ff>") + transparencyMode;
        }
        else if (type == "toggledownsamplingtechnique")
        {
            DownscalingTechnique technique = GetNextEnumValue(fSession.fDownScalingTechnique);

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
            switch (fWindow.GetWindowMode())
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
                case OIV::FilterType::None:
                    result.resValue = LLUTILS_TEXT("No filtering");
                    break;
                case OIV::FilterType::Linear:
                    result.resValue = LLUTILS_TEXT("Linear filtering");
                    break;
                case OIV::FilterType::Lanczos3:
                    result.resValue = LLUTILS_TEXT("Lanczos3 filtering");
                    break;
                case OIV::FilterType::Count:
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                    break;
            }
        }
    }

    void Viewer::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request,
                                       [[maybe_unused]] CommandManager::CommandResult& result)
    {
        auto type = request.args.GetArgValue("type");
        if (type == "imageinfo")  // Toggle image info
        {
            SetImageInfoVisible(!GetImageInfoVisible());
        }
        else if (type == "keybindings")  // Toggle keybindings
        {
            TextLabel* text = fLabelManager.GetTextLabel("keyBindings");
            if (text != nullptr)  //
            {
                fLabelManager.Remove("keyBindings");
                fRefreshOperation.Queue();
                return;
            }

            text         = fLabelManager.GetOrCreateTextLabel("keyBindings");
            auto message = MessageHelper::CreateKeyBindingsMessage(fConfiguredBindings, fConfiguredCommands);
            text->SetText(message);
            text->SetBackgroundColor({0, 0, 0, 216});
            text->SetFontSize(12);
            text->SetOutlineWidth(2);
            text->SetPosition({20, 60});
            text->SetFilterType(OIV::FilterType::None);
            text->SetImageRenderMode(OIV::ImageRenderMode::Overlay);
            text->SetScale({1.0, 1.0});
            text->SetOpacity(1.0);
            text->SetVisible(true);

            if (text->IsDirty())
                fRefreshOperation.Queue();
        }
        else if (type == "settings")  // Show settings
        {
            if (fShell.showSettings)
                fShell.showSettings();
        }
    }

    void Viewer::CMD_ShowSystemInfo([[maybe_unused]] const CommandManager::CommandRequest& request,
                                    [[maybe_unused]] CommandManager::CommandResult& result)
    {
        TextLabel* text = fLabelManager.GetTextLabel("systemInfo");
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
            .appName      = "OpenImageViewer",
            .appVersion   = OIV::FormatFullVersion(OIV::CurrentVersion),
            .gitHash      = OIV_GIT_SHORT_HASH,
            .buildType    = buildType,
            .renderer     = fRenderer.get(),
            .rendererType = fRendererType,
        });

        text->SetText(message);
        text->SetBackgroundColor({0, 0, 0, 216});
        text->SetFontSize(12);
        text->SetOutlineWidth(2);
        text->SetPosition({20, 60});
        text->SetFilterType(OIV::FilterType::None);
        text->SetImageRenderMode(OIV::ImageRenderMode::Overlay);
        text->SetScale({1.0, 1.0});
        text->SetOpacity(1.0);
        text->SetVisible(true);

        if (text->IsDirty())
            fRefreshOperation.Queue();
    }

    void Viewer::CMD_OpenFile([[maybe_unused]] const CommandManager::CommandRequest& request,
                              [[maybe_unused]] CommandManager::CommandResult& response)
    {
        std::string cmd = request.args.GetArgValue("cmd");
        if (cmd == "savefile")
        {
            LLUtils::native_string_type saveFilePath;
            LLUtils::native_string_type defaultFileName;
            if (IsImageOpen())
            {
                auto openedImage = fSession.fImageState.GetOpenedImage();
                if (!openedImage->sourceName.empty())
                    defaultFileName = openedImage->sourceName;
                else
                {
                    switch (openedImage->source)
                    {
                        case ImageSource::ClipboardText:
                            defaultFileName = LLUTILS_TEXT("text");
                            break;
                        case ImageSource::Clipboard:
                            defaultFileName = LLUTILS_TEXT("clipboard");
                            break;
                        default:
                            defaultFileName = LLUTILS_TEXT("image");
                            break;
                    }
                }

                auto result = LWS::FileDialog::Show(LWS::FileDialogType::SaveFile, fSaveComDlgFilters,
                                                    LLUTILS_TEXT("Save an image"), fWindow.GetWindow(),
                                                    fDefaultSaveFileExtension, fDefaultSaveFileFormatIndex,
                                                    defaultFileName, saveFilePath);

                if (result == LWS::FileDialogResult::Success)
                {
                    // Refresh full-resolution render pixels to bake pending transfers through cached stages.
                    // Source and geometric pixels remain available unchanged for original-texel inspection.
                    fSession.fImageState.Refresh(ImageProcessingStage::RenderFormat);
                    auto rasterized = fSession.fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat);
                    if (rasterized && !IMUtil::ImageUtil::HasAlphaChannelAndInUse(rasterized))
                        rasterized = IMUtil::ImageUtil::Convert(rasterized, IMCodec::TexelFormat::I_R8_G8_B8);
                    if (!rasterized)
                        response.resValue = LLUTILS_TEXT("Unable to prepare the image for saving.");
                    else if (fSession.fImageLoader.Encode(rasterized, saveFilePath) != IMCodec::ImageResult::Success)
                        response.resValue = LLUTILS_TEXT("Unable to encode the image in the selected format.");
                }
            }
        }

        else
        {
            LLUtils::native_string_type openFilePath;
            auto result = LWS::FileDialog::Show(LWS::FileDialogType::OpenFile, fOpenComDlgFilters,
                                                LLUTILS_TEXT("Open image"), fWindow.GetWindow(), {}, 0, {},
                                                openFilePath);

            if (result == LWS::FileDialogResult::Success)
                LoadFile(openFilePath, IMCodec::PluginTraverseMode::SuitablePlugin);
        }
    }

    void Viewer::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request,
                                          CommandManager::CommandResult& response)
    {
        const AxisAlignedTransformCommand command = ImageTransformCommandPolicy::ParseAxisAlignedTransform(
            request.args);

        if (command.HasTransform())
        {
            TransformImage(command.rotation, command.flip);
            response.resValue = ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
                fSession.fImageState.GetAxisAlignedRotation(), fSession.fImageState.GetAxisAlignedFlip());
        }
    }

    void Viewer::CMD_ToggleColorCorrection([[maybe_unused]] const CommandManager::CommandRequest& request,
                                           CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        if (ToggleColorCorrection())
            result.resValue = LLUTILS_TEXT("Reset color correction to previous");
        else
            result.resValue = LLUTILS_TEXT("Reset color correction to default");
    }

    void Viewer::CMD_SetWindowSize(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        const WindowSizeDecision decision = GetWindowSizeDecision(request.args);

        LWS::Result applied = LWS::Result::Success;
        switch (decision.mode)
        {
            case WindowSizeMode::Fullscreen:
                applied = fWindow.SetWindowMode(LWS::WindowMode::Fullscreen);
                break;
            case WindowSizeMode::MultiFullscreen:
                applied = fWindow.SetWindowMode(LWS::WindowMode::FullscreenAllMonitors);
                break;
            case WindowSizeMode::Maximized:
                // Use native maximization for "Window size entire screen" to preserve the normal restore placement.
                // Resizing to the monitor bounds would also fill the usable area, but overwrite that placement
                // and lose the expected maximize/restore behavior.
                applied = fWindow.GetWindow().RequestShowState(LWS::WindowShowState::Maximized);
                break;
            case WindowSizeMode::Windowed:
                applied = fWindow.SetWindowMode(LWS::WindowMode::Windowed);
                if (applied == LWS::Result::Success)
                    applied = fWindow.GetWindow().RequestShowState(LWS::WindowShowState::Restored);
                if (applied == LWS::Result::Success)
                    applied = fWindow.GetWindow().RequestPlacement(
                        {.position   = fPlatform.Supports(LWS::PlatformFeature::AbsoluteWindowPosition).value_or(false)
                                           ? std::optional(decision.position)
                                           : std::nullopt,
                         .clientSize = LWS::LogicalSize{decision.size.x, decision.size.y}});
                break;
            case WindowSizeMode::None:
                break;
        }

        if (applied != LWS::Result::Success)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "The requested window mode is unavailable");
        result.resValue = LLUtils::StringUtility::ToNativeString(request.displayName);
    }

    void Viewer::CMD_SortFiles(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        const SortCommandDecision decision = SortCommandPolicy::Decide(request.args,
                                                                       fSession.fFileSorter.GetSortType());

        if (decision.valid)
        {
            if (decision.reverseDirection)
                fSession.fFileSorter.SetActiveSortDirection(
                    SortCommandPolicy::Reverse(fSession.fFileSorter.GetActiveSortDirection()));
            else
                fSession.fFileSorter.SetSortType(decision.sortType);
        }

        SortFolderFileList();
        result.resValue = SortCommandPolicy::FormatSortResult(request.displayName,
                                                              fSession.fFileSorter.GetActiveSortDirection());
    }

    void Viewer::CMD_Sequencer(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        auto openedImage = fSession.fImageState.GetOpenedImage();
        if (openedImage != nullptr &&
            openedImage->pixels->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame)
        {
            if (SequencerPolicy::IsChangeSpeedCommand(request.args))
            {
                auto change = fSession.fSequencerPolicy.ChangeSpeed(
                    SequencerPolicy::ParseSpeedChangePercent(request.args));
                if (change.retimed)
                    UpdateSequencerInterval();
                result.resValue = std::move(change.message);
            }
        }
    }

    void Viewer::UpdateSequencerInterval()
    {
        if (fSequencerTimer.GetInterval() != 0)
        {
            // Retiming has already preserved the unplayed fraction. A due frame needs a prompt
            // callback; zero would stop the platform timer instead of advancing the animation.
            fSequencerTimer.SetInterval(std::max(1u, fSession.fSequencerPolicy.RemainingFrameMs()));
        }
    }

    void Viewer::CMD_DeleteFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void Viewer::CMD_ColorCorrection(const CommandManager::CommandRequest& request,
                                     CommandManager::CommandResult& result)
    {
        const ColorCorrectionCommand command = ColorCorrectionCommandPolicy::Parse(request.args);
        if (command.IsValid() == false)
            return;

        double* value = nullptr;
        switch (command.channel)
        {
            case ColorCorrectionChannel::Gamma:
                value = &fSession.fColorExposure.gamma;
                break;
            case ColorCorrectionChannel::Exposure:
                value = &fSession.fColorExposure.exposure;
                break;
            case ColorCorrectionChannel::Offset:
                value = &fSession.fColorExposure.offset;
                break;
            case ColorCorrectionChannel::Saturation:
                value = &fSession.fColorExposure.saturation;
                break;
            case ColorCorrectionChannel::Contrast:
                value = &fSession.fColorExposure.contrast;
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

    void Viewer::CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void Viewer::CMD_CopyToClipboard(const CommandManager::CommandRequest& request,
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

    void Viewer::CMD_PasteFromClipboard([[maybe_unused]] const CommandManager::CommandRequest& request,
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

    void Viewer::CMD_ImageManipulation(const CommandManager::CommandRequest& request,
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
            if (fSession.fImageState.GetOpenedImage() != nullptr)
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

    void Viewer::CMD_Placement(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void Viewer::CMD_Navigate(const CommandManager::CommandRequest& request,
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

    void Viewer::CMD_Shell(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        result.resValue = ShellCommandHandler::Execute(request, GetOpenedFileName(),
                                                       fSession.fImageState.GetOpenedImage());
    }

    template <typename T>
    LLUtils::native_string_type IntToHex(T val)
    {
        LLUtils::native_stringstream ss;
        ss << std::setfill(LLUTILS_TEXT("0")[0]) << std::setw(sizeof(T) * 2) << std::hex << val;

        return ss.str();
    }

    void Viewer::AddCommandsAndKeyBindings()
    {
        using namespace std;
        using namespace placeholders;

        const auto directory = fResources / "Configuration";
        fConfiguredCommands  = ConfigurationLoader::LoadCommandGroups(directory);
        fConfiguredBindings  = ConfigurationLoader::LoadKeyBindings(directory);
        CommandRegistry::AddConfiguredCommands(fCommandController.GetCommandManager(), fConfiguredCommands);
        AddPlatformKeyBindings();
        std::vector<CommandController::CommandRegistration> registrations{
            {"cmd_color_correction", std::bind(&Viewer::CMD_ColorCorrection, this, _1, _2)},
            {"cmd_view_state", std::bind(&Viewer::CMD_ViewState, this, _1, _2)},
            {"cmd_toggle_correction", std::bind(&Viewer::CMD_ToggleColorCorrection, this, _1, _2)},
            {"cmd_toggle_keybindings", std::bind(&Viewer::CMD_ToggleKeyBindings, this, _1, _2)},
            {"cmd_show_system_info", std::bind(&Viewer::CMD_ShowSystemInfo, this, _1, _2)},
            {"cmd_axis_aligned_transform", std::bind(&Viewer::CMD_AxisAlignedTransform, this, _1, _2)},
            {"cmd_open_file", std::bind(&Viewer::CMD_OpenFile, this, _1, _2)},
            {"cmd_zoom", std::bind(&Viewer::CMD_Zoom, this, _1, _2)},
            {"cmd_pan", std::bind(&Viewer::CMD_Pan, this, _1, _2)},
            {"cmd_placement", std::bind(&Viewer::CMD_Placement, this, _1, _2)},
            {"cmd_copyToClipboard", std::bind(&Viewer::CMD_CopyToClipboard, this, _1, _2)},
            {"cmd_pasteFromClipboard", std::bind(&Viewer::CMD_PasteFromClipboard, this, _1, _2)},
            {"cmd_imageManipulation", std::bind(&Viewer::CMD_ImageManipulation, this, _1, _2)},
            {"cmd_navigate", std::bind(&Viewer::CMD_Navigate, this, _1, _2)},
            {"cmd_file_associations", std::bind(&Viewer::CMD_FileAssociations, this, _1, _2)},
            {"cmd_shell", std::bind(&Viewer::CMD_Shell, this, _1, _2)},
            {"cmd_delete_file", std::bind(&Viewer::CMD_DeleteFile, this, _1, _2)},
            {"cmd_set_window_size", std::bind(&Viewer::CMD_SetWindowSize, this, _1, _2)},
            {"cmd_sort_files", std::bind(&Viewer::CMD_SortFiles, this, _1, _2)},
            {"cmd_sequencer", std::bind(&Viewer::CMD_Sequencer, this, _1, _2)}};
        fCommandController.SetRequestPredicate(fShell.commandAllowed);
        fCommandController.AddCommandCallbacks(registrations);
    }

    bool Viewer::ExecutePredefinedCommand(std::string command)
    {
        return fCommandController.ExecutePredefinedCommand(command);
    }

    bool Viewer::ExecuteCommandInternal(const CommandRequestIntenal& requestInternal)
    {
        return fCommandController.ExecuteCommandInternal(requestInternal.commandName, requestInternal.args);
    }

    bool Viewer::ExecuteCommand(const CommandManager::CommandRequest& request)
    {
        return fCommandController.ExecuteCommand(request);
    }
}  // namespace OIV
