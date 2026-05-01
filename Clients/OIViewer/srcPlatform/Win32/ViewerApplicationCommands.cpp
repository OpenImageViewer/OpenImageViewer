#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

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

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "globals.h"
#include "ConfigurationLoader.h"
#include "CommandRegistry.h"
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
#include <oivappcore/OIVImageHelper.h>
#include <oivappcore/SelectionWorkflowPolicy.h>
#include <oivappcore/SequencerPolicy.h>
#include <oivappcore/SortCommandPolicy.h>
#include <oivappcore/SubImagePolicy.h>
#include <oivappcore/ViewActionController.h>
#include <oivappcore/ViewCommandPolicy.h>
#include <oivappcore/ViewerPresentationPolicy.h>
#include <oivshared/PixelHelper.h>
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

    void ViewerApplication::CMD_Zoom(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        if (IsImageOpen())
        {
            const ZoomCommand command = ViewCommandPolicy::ParseZoom(request.args);
            ZoomInternal(command.amount, command.centerX, command.centerY);
            result.resValue = ViewCommandPolicy::FormatZoomResult(GetScale());
        }
    }

    void ViewerApplication::CMD_ViewState(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_OpenFile([[maybe_unused]] const CommandManager::CommandRequest& request,
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

    void ViewerApplication::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request,
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

    void ViewerApplication::CMD_ToggleColorCorrection([[maybe_unused]] const CommandManager::CommandRequest& request,
                                            CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;

        if (ToggleColorCorrection())
            result.resValue = L"Reset color correction to previous";
        else
            result.resValue = L"Reset color correction to default";
    }

    void ViewerApplication::CMD_SetWindowSize(const CommandManager::CommandRequest& request,
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

    void ViewerApplication::CMD_SortFiles(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_Sequencer(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_DeleteFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

        *value = ColorCorrectionCommandPolicy::Apply(*value, command.operation, command.value);
        result.resValue = ColorCorrectionCommandPolicy::FormatResult(command, *value);
        UpdateExposure();
    }

    void ViewerApplication::CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_PasteFromClipboard([[maybe_unused]] const CommandManager::CommandRequest& request,
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

    void ViewerApplication::CMD_ImageManipulation(const CommandManager::CommandRequest& request,
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

    void ViewerApplication::CMD_Placement(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::CMD_Shell(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

    void ViewerApplication::AddCommandsAndKeyBindings()
    {
        using namespace std;
        using namespace placeholders;

        fCommandController.AddConfiguredCommandsAndKeyBindings(fKeyBindings);
        fCommandController.AddCommandCallbacks(
            {{"cmd_color_correction", std::bind(&ViewerApplication::CMD_ColorCorrection, this, _1, _2)},
             {"cmd_view_state", std::bind(&ViewerApplication::CMD_ViewState, this, _1, _2)},
             {"cmd_toggle_correction", std::bind(&ViewerApplication::CMD_ToggleColorCorrection, this, _1, _2)},
             {"cmd_toggle_keybindings", std::bind(&ViewerApplication::CMD_ToggleKeyBindings, this, _1, _2)},
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
