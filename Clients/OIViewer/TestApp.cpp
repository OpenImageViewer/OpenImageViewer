#include <limits>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "TestApp.h"

#include <Windows.h>
#include <Version.h>

#include <functions.h>
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
#include "Helpers/MessageHelper.h"
#include "Helpers/PhotoshopFinder.h"


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
#include "Helpers/PixelHelper.h"
#include "ExceptionHandler.h"
#include <ImageUtil/ImageUtil.h>

#include "resource.h"

namespace OIV
{
    void TestApp::CMD_Zoom(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {

        if (IsImageOpen())
        {
            using namespace LLUtils;
            using namespace std;


            string cxStr = request.args.GetArgValue("cx");
            string cyStr = request.args.GetArgValue("cy");
            double val = std::atof(request.args.GetArgValue("val").c_str());

            int32_t cx = cxStr.empty() ? -1 : std::atoi(cxStr.c_str());
            int32_t cy = cyStr.empty() ? -1 : std::atoi(cyStr.c_str());

            ZoomInternal(val, cx, cy);

            wstringstream ss;
            ss << "<textcolor=#ff8930>Zoom <textcolor=#7672ff>("
                << fixed << setprecision(2) << GetScale() * 100.0 << "%)";

            result.resValue = ss.str();
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
            result.resValue = fImageState.GetUseRainbowNormalization() ? L"Rainbow normalization" : L"Grayscale normalization";
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
        else if (type == "toggleFullScreen") // Toggle full screen
        {
			ToggleFullScreen(false);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleMultiFullScreen") //Toggle multi full screen
        {
			ToggleFullScreen(true);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleresetoffset")
        {
            fResetTransformationMode = static_cast<ResetTransformationMode>((static_cast<int>(fResetTransformationMode) + 1) % static_cast<int>(ResetTransformationMode::Count));
            result.resValue = fResetTransformationMode == ResetTransformationMode::DoNothing ? L"Don't auto reset image state" : L"Auto reset image state";
        }
        else if (type == "toggletransparencymode")
        {
            fTransparencyMode = static_cast<OIV_PROP_TransparencyMode>( (fTransparencyMode + 1) % static_cast<int>(OIV_PROP_TransparencyMode::TM_Count));
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

            result.resValue = userMessage + L"<textcolor=#7672ff>"+ transparencyMode;
            
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
            result.resValue = DefaultTextKeyColorTag + L"Downscaling technique: " + DefaultTextValueColorTag + downscaleTechnique;

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
        netsettings_SaveSettings_func  SaveSettings;
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
                [[maybe_unused]] auto directory = AddDllDirectory((netsettingsPath.lexically_normal().wstring() + L"\\"). c_str());
                HMODULE dllModule = LoadLibrary(cliAdapterPath.c_str());
                if (dllModule != nullptr)
                {
                    settingsContext.Create = reinterpret_cast<netsettings_Create_func>(GetProcAddress(dllModule, "netsettings_Create"));
                    settingsContext.SetVisible = reinterpret_cast<netsettings_SetVisible_func>(GetProcAddress(dllModule, "netsettings_SetVisible"));
                    settingsContext.SaveSettings = reinterpret_cast<netsettings_SaveSettings_func>(GetProcAddress(dllModule, "netsettings_SaveUserSettings"));

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
                    LLUtils::Logger::GetSingleton().Log(std::wstring(L"Cannot load Netsettings extension, error: ") + LLUtils::PlatformUtility::GetLastErrorAsString<wchar_t>());
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

    void TestApp::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request, [[maybe_unused]] CommandManager::CommandResult& result)
    {
        auto type = request.args.GetArgValue("type");
        if (type == "imageinfo") // Toggle image info
        {
            SetImageInfoVisible(!GetImageInfoVisible());
        }
        else if (type == "keybindings")// Toggle keybindings
        {
            OIVTextImage* text = fLabelManager.GetTextLabel("keyBindings");
            if (text != nullptr) //
            {
                fLabelManager.Remove("keyBindings");
                fRefreshOperation.Queue();
                return;
            }

            text = fLabelManager.GetOrCreateTextLabel("keyBindings");
            auto message = MessageHelper::CreateKeyBindingsMessage();
            
            text->SetText(message);
            text->SetBackgroundColor({ 0, 0, 0, 216 });
            text->SetFontPath(LabelManager::sFixedFontPath);
            text->SetFontSize(12);
            text->SetOutlineWidth(2);
            text->SetPosition({ 20,60 });
            text->SetFilterType(OIV_Filter_type::FT_None);
            text->SetImageRenderMode(IRM_Overlay);
            text->SetScale({ 1.0,1.0 });
            text->SetOpacity(1.0);
            text->SetVisible(true);

            if (text->IsDirty())
                fRefreshOperation.Queue();

        }
        else if (type == "settings") // Show settings
        {
            ShowSettings();
        }
    }

    void TestApp::CMD_OpenFile([[maybe_unused]] const CommandManager::CommandRequest& request, [[maybe_unused]] CommandManager::CommandResult& response)
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

                auto result = FileDialog::Show(FileDialogType::SaveFile, fSaveComDlgFilters.GetFilters(), L"Save an image"
                    , fWindow.GetHandle(), L"*." + fDefaultSaveFileExtension, fDefaultSaveFileFormatIndex, defaultFileName, saveFilePath);

                if (result == FileDialogResult::Success)
                {
                    std::wstring extension = LLUtils::StringUtility::ToLower(std::filesystem::path(saveFilePath).extension().wstring());
                    std::wstring_view sv(extension);

                    if (sv.empty() == false)
                        sv = sv.substr(1);

                    auto rasterized = fImageState.GetImage(ImageChainStage::Rasterized)->GetImage();

                    if (IMUtil::ImageUtil::HasAlphaChannelAndInUse(rasterized) == false)
                        rasterized = IMUtil::ImageUtil::Convert(rasterized, IMCodec::TexelFormat::I_R8_G8_B8); // Get rid of the Alpha channel


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
            auto result = FileDialog::Show(FileDialogType::OpenFile, fOpenComDlgFilters.GetFilters(), L"Open image", fWindow.GetHandle(), {}, 0, {}, openFilePath);

            if (result == FileDialogResult::Success)
                LoadFile(openFilePath, IMCodec::PluginTraverseMode::NoTraverse);
        }
    }

    void TestApp::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request, CommandManager::CommandResult& response)
    {
        IMUtil::AxisAlignedRotation rotation = IMUtil::AxisAlignedRotation::None;
        IMUtil::AxisAlignedFlip flip = IMUtil::AxisAlignedFlip::None;

        std::string type = request.args.GetArgValue("type");

        if (false);
        else if (type == "hflip")
            flip = IMUtil::AxisAlignedFlip::Horizontal;
        else if (type == "vflip")
            flip = IMUtil::AxisAlignedFlip::Vertical;
        else if (type == "rotatecw")
            rotation = IMUtil::AxisAlignedRotation::Rotate90CW; 
        else if (type == "rotateccw")
            rotation = IMUtil::AxisAlignedRotation::Rotate90CCW;


        if (rotation != IMUtil::AxisAlignedRotation::None || flip != IMUtil::AxisAlignedFlip::None)
        {
            TransformImage(rotation, flip);

            std::wstring rotation;
            switch (fImageState.GetAxisAlignedRotation())
            {
            case IMUtil::AxisAlignedRotation::Rotate90CW:
                rotation = L"90 degrees clockwise";
                break;
            case IMUtil::AxisAlignedRotation::Rotate180:
                rotation = L"180 degrees";
                break;
            case IMUtil::AxisAlignedRotation::Rotate90CCW:
                rotation = L"90 degrees counter clockwise";
                break;
            case IMUtil::AxisAlignedRotation::None:
                break;
            }
            if (rotation.empty() == false)
                response.resValue += std::wstring(L"Rotation <textcolor=#7672ff>(") + rotation +  L')';


            std::wstring flip;

            switch (fImageState.GetAxisAlignedFlip())
            {
            case IMUtil::AxisAlignedFlip::Horizontal:
                flip = L"horizontal";
                break;
            case IMUtil::AxisAlignedFlip::Vertical:
                flip = L"vertical";
                break;
            case IMUtil::AxisAlignedFlip::None:
                break;
            }

            if (flip.empty() == false)
            {
                if (rotation.empty() == false)
                    response.resValue += L'\n';

                response.resValue += std::wstring(L"<textcolor=#ff8930>Flip <textcolor=#7672ff>(") + flip + L')';
            }

            if (flip.empty() == true && rotation.empty() == true)
            {
                response.resValue = L"No transformation";
            }

            
            //response.resValue = LLUtils::StringUtility::ToWString(request.description);
        }
    }


    void TestApp::CMD_ToggleColorCorrection([[maybe_unused]] const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        
        if (ToggleColorCorrection())
            result.resValue = L"Reset color correction to previous";
        else
            result.resValue = L"Reset color correction to default";
    }

    void TestApp::CMD_SetWindowSize(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        string size_type = request.args.GetArgValue("size_type");

        if (size_type == "fullscreen")
        {
            fWindow.SetFullScreenState(::Win32::FullSceenState::SingleScreen);
        }
        else if (size_type == "multifullscreen")
        {
            fWindow.SetFullScreenState(::Win32::FullSceenState::MultiScreen);
        }
        else
        {
            if (fWindow.GetFullScreenState() != ::Win32::FullSceenState::Windowed)
                fWindow.SetFullScreenState(::Win32::FullSceenState::Windowed);
            string widthStr = request.args.GetArgValue("width");
            string heightStr = request.args.GetArgValue("height");

            double width = std::stod(widthStr);
            double height = std::stod(heightStr);


            const int workingAreaWidth = fCurrentMonitorProperties.monitorInfo.rcWork.right - fCurrentMonitorProperties.monitorInfo.rcWork.left;
            const int workingAreaHeight = fCurrentMonitorProperties.monitorInfo.rcWork.bottom - fCurrentMonitorProperties.monitorInfo.rcWork.top;

            int finalWidth;
            int finalHeight;

            if (size_type == "absolute")
            {
                finalWidth = static_cast<int>(std::round(width));
                finalHeight = static_cast<int>(std::round(height));
            }

            else if (size_type == "relative")
            {
                int workingAreaWidth = fCurrentMonitorProperties.monitorInfo.rcWork.right - fCurrentMonitorProperties.monitorInfo.rcWork.left;
                int workingAreaHeight = fCurrentMonitorProperties.monitorInfo.rcWork.bottom - fCurrentMonitorProperties.monitorInfo.rcWork.top;
                double realtiveWidth = width * workingAreaWidth / 100.0;
                double realtiveHeight = height * workingAreaHeight / 100.0;

                finalWidth = static_cast<int>(std::round(realtiveWidth));
                finalHeight = static_cast<int>(std::round(realtiveHeight));
            }

            finalWidth = std::min(workingAreaWidth, finalWidth);
            finalHeight = std::min(workingAreaHeight, finalHeight);


            auto windowNewSize = PointI32{ finalWidth, finalHeight };
            auto windowCurrentSize = fWindow.GetWindowSize();
            auto currentPos = fWindow.GetPosition();
            auto displacedPos = currentPos - (windowNewSize - windowCurrentSize) / 2;

            displacedPos.x = std::clamp<int32_t>(displacedPos.x,
                fCurrentMonitorProperties.monitorInfo.rcWork.left, fCurrentMonitorProperties.monitorInfo.rcWork.right - windowNewSize.x);

            displacedPos.y = std::clamp<int32_t>(displacedPos.y,
                fCurrentMonitorProperties.monitorInfo.rcWork.top, fCurrentMonitorProperties.monitorInfo.rcWork. bottom - windowNewSize.y);
            
            if (displacedPos != currentPos)
                fWindow.SetPosition(displacedPos.x, displacedPos.y);

            fWindow.SetSize(finalWidth, finalHeight);
        }
        result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
    }

    void TestApp::CMD_SortFiles(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        string sort_type = request.args.GetArgValue("type");

        bool reverseDirection = false;
        if (sort_type == "name")
        {
            if (fFileSorter.GetSortType() == FileSorter::SortType::Name)
                reverseDirection = true;
            else 
                fFileSorter.SetSortType(FileSorter::SortType::Name);
        }
            
        else if (sort_type == "date")
        {
            if (fFileSorter.GetSortType() == FileSorter::SortType::Date)
                reverseDirection = true;
            else 
                fFileSorter.SetSortType(FileSorter::SortType::Date);
        }
        
        else if (sort_type == "extension")
        {
            if (fFileSorter.GetSortType() == FileSorter::SortType::Extension)
                reverseDirection = true;
            else 
                fFileSorter.SetSortType(FileSorter::SortType::Extension);
        }

        if (reverseDirection)
        {
            fFileSorter.SetActiveSortDirection(fFileSorter.GetActiveSortDirection() == FileSorter::SortDirection::Ascending ?
                FileSorter::SortDirection::Descending : FileSorter::SortDirection::Ascending);
        }

        SortFileList();
        UpdateOpenedFileIndex();

        auto userMessage = LLUtils::StringUtility::ToWString(request.displayName);
        
        userMessage += std::wstring(L" [") + (fFileSorter.GetActiveSortDirection() == FileSorter::SortDirection::Ascending ? L"Ascending" : L"Descending")
            + L"]";

        result.resValue = userMessage;
    }


    void TestApp::CMD_Sequencer(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        string command = request.args.GetArgValue("cmd");
        auto openedImage = fImageState.GetOpenedImage();
        if (openedImage != nullptr && openedImage->GetImage()->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame)
        {
            if (command == "changespeed")
            {
                auto amount = request.args.GetArgValue("amount");
                double amountVal = std::stod(amount, nullptr) / 100.0;

                fCurrentSequencerSpeed *= 1 + amountVal;

                wstringstream ss;
                ss << "<textcolor=#ff8930>" << L"Animation speed" << L"<textcolor=#7672ff> ("
                    << fixed << setprecision(2) << fCurrentSequencerSpeed * 100 << "%)";

                result.resValue = ss.str();
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



    void TestApp::CMD_ColorCorrection(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace std;
        string type = request.args.GetArgValue("type");
        string op = request.args.GetArgValue("op");;
        string val = request.args.GetArgValue("val");;


        bool validValue = true;
        double newValue;

        if (type == "gamma")
            newValue = PerformColorOp(fColorExposure.gamma, op, val);
        else if (type == "exposure")
            newValue = PerformColorOp(fColorExposure.exposure, op, val);
        else if (type == "offset")
            newValue = PerformColorOp(fColorExposure.offset, op, val);
        else if (type == "saturation")
            newValue = PerformColorOp(fColorExposure.saturation, op, val);
        else if (type == "contrast")
            newValue = PerformColorOp(fColorExposure.contrast, op, val);
        else
        {
            validValue = false;
        }
        if (validValue == true)
        {

            std::wstringstream ss;

            ss << L"<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(type) << L"<textcolor=#7672ff>" << " "
                << LLUtils::StringUtility::ToWString(op) << L" " << LLUtils::StringUtility::ToWString(val);

            if (op == "increase" || op == "decrease")
                ss << "%";


            ss << "<textcolor=#00ff00> (" << std::fixed << std::setprecision(0) << newValue * 100 << "%)";
            result.resValue = ss.str();
            UpdateExposure();
        }
    }

    void TestApp::CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace std;
        string direction = request.args.GetArgValue("direction");
        string amount = request.args.GetArgValue("amount");;

        double amountVal = std::stod(amount, nullptr);



        if (direction == "up")
            Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(amountVal)));
        else if (direction == "down")
            Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(-amountVal)));
        else if (direction == "left")
            Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(amountVal), 0));
        else if (direction == "right")
            Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(-amountVal), 0));


        std::wstringstream ss;

        ss << "<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(request.displayName) << "<textcolor=#7672ff>" << " (" << amountVal << " pixels)";

        result.resValue = ss.str();

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
                result.resValue = std::wstring(L"Cannot copy to clipboard - ") + GetErrorString(res);
            else 
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
            
        }
        else if (cmd == "cut")
        {
            OperationResult res = CutSelectedArea();
            if (res != OperationResult::Success)
                result.resValue = std::wstring(L"Cannot cut selected area - ") + GetErrorString(res);
            else
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }
    }

    std::wstring TestApp::GetErrorString(OperationResult res) const
    {
        switch (res)
        {
            case OperationResult::NoDataFound:
            return L"No Image loaded";
            case OperationResult::Success:
            return L"Success";
            case OperationResult::NoSelection:
                return L"No selection";
            case OperationResult::UnkownError:
            default:
                return L"Unknown error";
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
                result.resValue = std::wstring(L"Cannot crop selected area - ") + GetErrorString(res);
            else
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }

        else if (cmd == "selectAll" )
        {
            if (fImageState.GetOpenedImage() != nullptr)
            {
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
                using namespace LLUtils;
                RectI32 imageInScreenSpace = static_cast<LLUtils::RectI32>(ImageToClient({ { 0.0,0.0 }, { GetImageSize(ImageSizeType::Transformed) } }));

                fRefreshOperation.Begin();
                fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, { 0,0 });
                fSelectionRect.SetSelection(SelectionRect::Operation::BeginDrag, imageInScreenSpace.GetCorner(Corner::TopLeft));
                fSelectionRect.SetSelection(SelectionRect::Operation::Drag, imageInScreenSpace.GetCorner(Corner::BottomRight));
                fSelectionRect.SetSelection(SelectionRect::Operation::EndDrag, imageInScreenSpace.GetCorner(Corner::BottomRight));
                
                SetImageSpaceSelection(LLUtils::RectI32{ { 0, 0 }, LLUtils::PointI32 { GetImageSize(ImageSizeType::Transformed) } });
   
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
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");


        if (cmd == "originalSize")
            SetOriginalSize();
        else if (cmd == "fitToScreen")
            FitToClientAreaAndCenter();
        else if (cmd == "center")
            Center();

        wstringstream ss;

        ss << "<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(request.displayName);// << "<textcolor=#7672ff>" << " (" << amountVal << " pixels)";

        result.resValue = ss.str();

    }

    void TestApp::CMD_Navigate(const CommandManager::CommandRequest& request,[[maybe_unused]] CommandManager::CommandResult& result)
    {
        using namespace std;
        const string amountStr = request.args.GetArgValue("amount");
        const string isSubImage = request.args.GetArgValue("subimage");
        FileIndexType amount = amountStr == "start" ? FileIndexStart : amountStr == "end" ? FileIndexEnd : std::stoi(amountStr, nullptr);
        if (isSubImage == "true")
        {
            
            auto& imageList = fWindow.GetImageControl().GetImageList();
            const auto numElements = imageList.GetNumberOfElements();
            if (numElements > 0)
            {
                FileIndexType nextIndex = LLUtils::Math::Modulu<FileIndexType>(imageList.GetSelected() + amount, numElements);
                imageList.SetSelected(static_cast<int>(nextIndex));
            }
        }
        else
        {
            JumpFiles(amount);
        }
    }

    void TestApp::CMD_Shell(const CommandManager::CommandRequest& request,
        CommandManager::CommandResult& result)
    {
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");
        result.resValue = LLUtils::StringUtility::ToWString(request.displayName);

        if (cmd == "newWindow")
        {
            using namespace LLUtils;
            //Open new window
            ShellExecute(nullptr, L"open", StringUtility::ToNativeString(PlatformUtility::GetExePath()).c_str(), GetOpenedFileName().c_str(), nullptr, SW_SHOWDEFAULT);
        }
        else if (cmd == "openPhotoshop")
        {
            std::wstring photoshopApplicationPath = PhotoShopFinder::FindPhotoShop();
            if (photoshopApplicationPath.empty() == false)
                ShellExecute(nullptr, L"open", photoshopApplicationPath.c_str(), GetOpenedFileName().c_str(), nullptr, SW_SHOWDEFAULT);
        }
        else if (cmd == "openWithGoogleMaps")
        {
            auto openedIMage = fImageState.GetOpenedImage();
            if (openedIMage != nullptr)
            {
                auto image = openedIMage->GetImage();
                if (image != nullptr && openedIMage->GetMetaData()->exifData.latitude != std::numeric_limits<double>::max())
                {
                    std::wstringstream ss;
                    const auto& exifData = openedIMage->GetMetaData()->exifData;

                    ss << "https://www.google.com/maps/place/@" << exifData.latitude << "," << exifData.longitude << ",1000m/data=!3m1!1e3";

                    auto str = ss.str();
                    ShellExecute(nullptr, L"open", ss.str().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
                }
                else
                {
                    result.resValue = L"No geo location data found";

                }
            }
        }
        else if (cmd == "containingFolder")
        {
            if (GetOpenedFileName().empty() == false)
                ::Win32::Win32Helper::BrowseToFile(GetOpenedFileName());
        }
        else if (cmd == "openWith")
        {
            if (GetOpenedFileName().empty() == false)
                ShellExecute(nullptr, L"open", LLUtils::StringUtility::ToNativeString(request.args.GetArgValue("app")).c_str() , GetOpenedFileName().c_str(), nullptr, SW_SHOWDEFAULT);
                
        }
    }

    template< typename T >
    std::wstring IntToHex(T val)
    {
        std::wstringstream ss;
        ss 
            << std::setfill(L'0') << std::setw(sizeof(T) * 2)
            << std::hex << val;

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
            ss << OIV_VERSION_MAJOR << dot << OIV_VERSION_MINOR << dot << OIV_VERSION_BUILD << dot << OIV_VERSION_REVISION;
            return ss.str();

        };

		return GetAppDataFolder() + GetVersionAsString() +  L"/oiv.log";
	}


    void TestApp::HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args, std::wstring seperatedCallStack)
    {
        using namespace std;
        wstringstream ss;
        std::wstring source = isFromLibrary ? L"OIV library" : L"OIV viewer";
        const wstring introMessage = LLUtils::Exception::ExceptionErrorCodeToString(args.errorCode) + L" exception has occured at " + args.functionName + L" at " + source + L".\nDescription: " + args.description ;
        const wstring displayMessage = introMessage + L"\nPlease refer to the log file [" + mLogFile.GetLogPath() + L"] for more information"; 
            
		ss << L"\n==================================================================================================\n";
        ss << introMessage << endl;

        if (args.systemErrorMessage.empty() == false)
            ss << "System error: " << args.systemErrorMessage;

        
        ss << "call stack:" << endl;

        if (seperatedCallStack.empty() == true)
            ss << LLUtils::Exception::FormatStackTrace(args.stackTrace, args.exceptionmode == LLUtils::Exception::Mode::Error ? 3 : 0xFFF);
        else
            ss << seperatedCallStack;

		mLogFile.Log(ss.str());
       // if (args.exceptionmode == LLUtils::Exception::Mode::Error)
         //   MessageBoxW(IsMainThread() ? fWindow.GetHandle() : nullptr, displayMessage.c_str(), L"Unhandled exception has occured.", MB_OK | MB_APPLMODAL);
        //DebugBreak();
    }

    TestApp::~TestApp()
    {
        if (fCountingColorsThread.joinable())
            fCountingColorsThread.join();

        RemoveExceptionHandler();
    }


    TestApp::TestApp()
        : fRefreshTimer(std::bind(&TestApp::OnRefreshTimer, this))
        , fRefreshOperation(std::bind(&TestApp::OnRefresh, this))
        , fPreserveImageSpaceSelection(std::bind(&TestApp::OnPreserveSelectionRect, this))
        , fSelectionRect(std::bind(&TestApp::OnSelectionRectChanged, this,std::placeholders::_1, std::placeholders::_2))
        , fVirtualStatusBar(&fLabelManager, std::bind(&TestApp::OnLabelRefreshRequest, this))
        , fFreeType(std::make_unique<FreeType::FreeTypeConnector>())
        , fLabelManager(fFreeType.get())
        //, fFileCache(&fImageLoader, std::bind(&TestApp::OnImageReady, this, std::placeholders::_1))
         
       
    {
       // LLUtils::Exception::SetThrowErrorsInDebug(false);
        EventManager::GetSingleton().MonitorChange.Add(std::bind(&TestApp::OnMonitorChanged, this, std::placeholders::_1));

        RegisterExceptionhandler();

        OIV_CMD_RegisterCallbacks_Request request;

        

        request.OnException = [](OIV_Exception_Args args, void* userPointer)
        {
            using namespace std;
            //Convert from C to C++
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
        {
                HandleException(false, args, {});
        }
        );
    }

    void TestApp::AddCommandsAndKeyBindings()
    {

        auto commandGroups = ConfigurationLoader::LoadCommandGroups();
        auto keyBindings = ConfigurationLoader::LoadKeyBindings();

        for (const auto& commandGroup : commandGroups)
        {
            fCommandManager.AddCommandGroup(
                { commandGroup.commandGroupID,
                commandGroup.commandDisplayName,
                commandGroup.commandName,
                commandGroup.arguments
                });
        }

        for (const auto& keyBindings : keyBindings)
        {
            fKeyBindings.AddBinding(LInput::KeyCombination::FromString(keyBindings.KeyCombinationName)
                , { keyBindings.GroupID, std::string(),std::string() });
        }
        using namespace std;
        using namespace placeholders;

        fCommandManager.AddCommand(CommandManager::Command("cmd_color_correction", std::bind(&TestApp::CMD_ColorCorrection, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_view_state", std::bind(&TestApp::CMD_ViewState, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_toggle_correction", std::bind(&TestApp::CMD_ToggleColorCorrection, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_toggle_keybindings", std::bind(&TestApp::CMD_ToggleKeyBindings, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_axis_aligned_transform", std::bind(&TestApp::CMD_AxisAlignedTransform, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_open_file", std::bind(&TestApp::CMD_OpenFile, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_zoom", std::bind(&TestApp::CMD_Zoom, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_pan", std::bind(&TestApp::CMD_Pan, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_placement", std::bind(&TestApp::CMD_Placement, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_copyToClipboard", std::bind(&TestApp::CMD_CopyToClipboard, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_pasteFromClipboard", std::bind(&TestApp::CMD_PasteFromClipboard, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_imageManipulation", std::bind(&TestApp::CMD_ImageManipulation, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_navigate", std::bind(&TestApp::CMD_Navigate, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_shell", std::bind(&TestApp::CMD_Shell, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_delete_file", std::bind(&TestApp::CMD_DeleteFile, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_set_window_size", std::bind(&TestApp::CMD_SetWindowSize, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_sort_files", std::bind(&TestApp::CMD_SortFiles, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_sequencer", std::bind(&TestApp::CMD_Sequencer, this, _1, _2)));



    }

    void TestApp::OnLabelRefreshRequest()
    {
        fRefreshOperation.Queue();
    }

    void TestApp::OnMonitorChanged(const EventManager::MonitorChangeEventParams& params)
    {
        fCurrentMonitorProperties = params.monitorDesc;

        //update the refresh rate.
        fRefreshRateTimes1000 = params.monitorDesc.DisplaySettings.dmDisplayFrequency == 59 ? 59940 : params.monitorDesc.DisplaySettings.dmDisplayFrequency * 1000;

        const LLUtils::PointF64 BaseDPI{ 96.0, 96.0 };

        // DPI adjustment. The mouse generates movement events as district units. 
        // To keep movement speed constant across several monitors in terms of distance,
        // DPI must be taken care into consideration.
        fDPIadjustmentFactor = LLUtils::PointF64{ static_cast<LLUtils::PointF64::point_type>(params.monitorDesc.DPIx),
            static_cast<LLUtils::PointF64::point_type>(params.monitorDesc.DPIy) } / BaseDPI;
    }

    void TestApp::ProbeForMonitorChange()
    {
        if (fIsFirstFrameDisplayed == true)
            fMonitorProvider.UpdateFromWindowHandle(fWindow.GetHandle());
    }

    void TestApp::PerformRefresh()
    {
        if (EnableFrameLimiter == true)
        {
            using namespace std::chrono;
            
            ProbeForMonitorChange();
            high_resolution_clock::time_point now = high_resolution_clock::now();
            auto windowTimeInMicroSeconds = 1'000'000'000 / fRefreshRateTimes1000;
            auto microsecSinceLastRefresh = duration_cast<microseconds>(now - fLastRefreshTime).count();

            if (microsecSinceLastRefresh > windowTimeInMicroSeconds)
            {
                fRefreshTimer.Enable(false);
                //Refresh immediately
                OIVCommands::Refresh();
                fLastRefreshTime = now;
                //Clear last image chain if exists, this operation is deffered to this moment to display the new image faster.
            }
            else
            {
                //Don't refresh now, restrat refresh timer
                if (fRefreshTimer.GetEnabled() == false)
                {
                    fRefreshTimer.SetDueTime(static_cast<DWORD>((windowTimeInMicroSeconds - microsecSinceLastRefresh) / 1000));
                    fRefreshTimer.Enable(true);
                }
            }
        }
        else
        {
            OIVCommands::Refresh();
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
        const static std::wstring cachedVersionString = OIV_TEXT("OpenImageViewer ") + std::to_wstring(OIV_VERSION_MAJOR) + L'.' + std::to_wstring(OIV_VERSION_MINOR) +
            (OIV_VERSION_REVISION != 0 ? (std::wstring(L".") + std::to_wstring(OIV_VERSION_REVISION)) : std::wstring{})


            // If not official release add revision and build number
#if OIV_OFFICIAL_RELEASE == 0

                + L"." + WIDEN(GIT_HASH_ID)
                +  L"." + std::to_wstring(OIV_VERSION_BUILD)
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
            switch (fImageState.GetOpenedImage()->GetImageSource())
            {
            case ImageSource::File:
            {
                auto decomposedPath = MessageFormatter::DecomposePath(GetOpenedFileName());
                std::wstringstream ss;
                if (GetAppActive() == true)
                {
                    ss << (fCurrentFileIndex == FileIndexStart ?
                        0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size()
                        << L" | ";
                }

                ss << decomposedPath.fileName << decomposedPath.extension
                    << " @ " << std::wstring_view(decomposedPath.parentPath.data(), decomposedPath.parentPath.length() - 1);

                title = ss.str() + L" - ";
            }
            break;
            case ImageSource::ClipboardText:
                title = L"Clipboard text - ";
                break;
            case ImageSource::Clipboard:
                title = L"Clipboard image - ";
                break;
            case ImageSource::GeneratedByLib:
                title = L"Internal image - ";
                break;
            default:
                title = L"Unknown image source - ";
                break;
            }
        }
        title += cachedVersionString;
        fWindow.SetTitle(title);
    }
    
   
    void TestApp::UnloadOpenedImaged()
    {
        fImageState.ClearAll();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void TestApp::DeleteOpenedFile(bool permanently)
    {
        size_t stringLength = GetOpenedFileName().length();
        auto buffer = std::make_unique<wchar_t[]>(stringLength + 2);

        memcpy(buffer.get(), GetOpenedFileName().c_str(), ( stringLength + 1) * sizeof(wchar_t));

        buffer.get()[stringLength + 1] = '\0';


        SHFILEOPSTRUCT file_op =
        {
              GetWindowHandle()
            , FO_DELETE
            , buffer.get()
            , nullptr
            , static_cast<FILEOP_FLAGS>(permanently ? 0 : FOF_ALLOWUNDO)
            , FALSE
            , nullptr
            , nullptr
        };

        auto fileNameToRemove = GetOpenedFileName();
        fRequestedFileForRemoval = fileNameToRemove;

        int shResult = SHFileOperation(&file_op);

        if (shResult != 0 )
        {
                //handle error
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
            SetUserMessage(L"File: " + MessageFormatter::FormatFilePath(GetOpenedFileName()),  static_cast<GroupID>(UserMessageGroups::SuccessfulFileLoad),MessageFlags::Interchangeable | MessageFlags::Moveable);
    }


    void TestApp::WatchCurrentFolder()
    {
        if (GetOpenedFileName().empty() == false)
        {
            std::wstring absoluteFolderPath = std::filesystem::path(GetOpenedFileName()).parent_path();
            if (absoluteFolderPath != fCurrentFolderWatched)
            {
                if (fCurrentFolderWatched.empty() == false)
                    fFileWatcher.RemoveFolder(fCurrentFolderWatched);

                fCurrentFolderWatched = absoluteFolderPath;

             fOpenedFileFolderID = fFileWatcher.AddFolder(absoluteFolderPath);
            }
        }
    }

    void TestApp::AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        // add an image to a windows control, so create a system compatiable image - flipped BGRA  in windows.

        // Convert to BGRA bitmap.
        auto bgraImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image , IMCodec::TexelFormat::I_B8_G8_R8_A8, false);  
        
        // Flip vertically.
        bgraImage = IMUtil::ImageUtil::Transform({ IMUtil::AxisAlignedRotation::None,IMUtil::AxisAlignedFlip::Vertical }, bgraImage);

        // Create 32 bit BGRA color image
        
        ::Win32::BitmapBuffer bitmapBuffer{};
        bitmapBuffer.bitsPerPixel = bgraImage->GetBitsPerTexel();
        bitmapBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(bgraImage->GetRowPitchInBytes(), sizeof(DWORD));
        LLUtils::Buffer colorBuffer(bgraImage->GetHeight() * bitmapBuffer.rowPitch);
        bitmapBuffer.buffer = colorBuffer.data();
        bitmapBuffer.height = bgraImage->GetHeight();
        bitmapBuffer.width = bgraImage->GetWidth();


        //Create 24 bit mask image.
        ::Win32::BitmapBuffer maskBuffer{};
        maskBuffer.bitsPerPixel = 24;
        maskBuffer.height = bgraImage->GetHeight();
        maskBuffer.width = bgraImage->GetWidth();
        maskBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(maskBuffer.width * maskBuffer.bitsPerPixel / CHAR_BIT, sizeof(DWORD));
        LLUtils::Buffer maskPixelsBuffer(maskBuffer.height * maskBuffer.rowPitch);
        maskBuffer.buffer = maskPixelsBuffer.data();

#pragma pack(push,1)

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
            //Create mask/color pairs for GDI painting.
            for (size_t x = 0; x < maskBuffer.width; x++)
            {
                Color24& destMask = reinterpret_cast<Color24*>(reinterpret_cast<uint8_t*>(maskPixelsBuffer.data()) + maskOffset)[x];
                Color32& destImage = reinterpret_cast<Color32*>(reinterpret_cast<uint8_t*>(colorBuffer.data()) + colorOffset)[x];
                const Color32& sourceColor = reinterpret_cast<const Color32*>(reinterpret_cast<const uint8_t*>(bgraImage->GetBuffer()) + sourceOffset)[x];

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
        ss << imageSlot + 1 << L'/' << totalImages << L"  " << bitmapBuffer.width << L" x " << bitmapBuffer.height << L" x " << bitmapBuffer.bitsPerPixel << L" BPP";

        fWindow.GetImageControl().GetImageList().SetImage({ imageSlot, ss.str(),
                std::make_shared<::Win32::BitmapSharedPtr::element_type>(bitmapBuffer)
                , std::make_shared<::Win32::BitmapSharedPtr::element_type>(maskBuffer) });
    }

    void TestApp::OnContextMenuTimer()
    {
        fContextMenuTimer.SetInterval(0);
        auto pos = ::Win32::Win32Helper::GetMouseCursorPosition();
        auto chosenItem = fContextMenu->Show(pos.x - 16  , pos.y +-16, AlignmentHorizontal::Center, AlignmentVertical::Center);

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
            return mainImage != nullptr
                && mainImage->GetSubImageGroupType() != ImageItemType::AnimationFrame
                && mainImage->GetNumSubImages() > 0;
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
            const auto isMainAnActualImage = mainImage->GetImage()->GetItemType() != ImageItemType::Container;
            const uint16_t totalImages = static_cast<uint16_t>(mainImage->GetImage()->GetNumSubImages() + (isMainAnActualImage ? 1 : 0));
            //Add the first image.
            uint16_t currentImage = 0;
            int largestIndex = -1;
            uint32_t largestSize = 0;
            if (isMainAnActualImage)
                AddImageToControl(mainImage->GetImage(), static_cast<uint16_t>(currentImage++), totalImages);

            if (mainImage->GetImage()->GetTotalPixels() > largestSize)
            {
                largestSize = mainImage->GetImage()->GetTotalPixels();
                largestIndex = -1;
            }


            for (uint16_t i = 0; i < numSubImages; i++)
            {
                auto currentSubImage = mainImage->GetImage()->GetSubImage(i);
                if (currentSubImage->GetTotalPixels() > largestSize)
                {
                    largestSize = currentSubImage->GetTotalPixels();
                    largestIndex = i;
                }
                
                AddImageToControl(currentSubImage, static_cast<uint16_t>(currentImage++), totalImages);
            }
            //Reset selected sub image when loading new set of subimages
            fWindow.GetImageControl().GetImageList().SetSelected(fDisplayBiggestSubImageOnLoad == true ? largestIndex : -1);
            fWindow.GetImageControl().RefreshScrollInfo();
        }
        else
        {
            fWindow.GetImageControl().GetImageList().Clear();
        }
    }


    bool TestApp::LoadFile(std::wstring filePath, IMCodec::PluginTraverseMode loaderFlags)
    {
        std::wstring normalizedPath = std::filesystem::path(filePath).lexically_normal().wstring();
      //  fFileCache.Add(normalizedPath);

        std::shared_ptr<OIVFileImage> file = std::make_shared<OIVFileImage>(normalizedPath);
        
        IMCodec::Parameters params = { {L"canvasWidth", (int)fWindow.GetClientSize().cx}, {L"canvasHeight", (int)fWindow.GetClientSize().cy} };

        ResultCode result = file->Load(&fImageLoader, loaderFlags, IMCodec::ImageLoadFlags::None, params);

        auto formattedFilePath = MessageFormatter::FormatFilePath(file->GetFileName()) + L"<textcolor=#ff8930>";

        using namespace std::string_literals;
        switch (result)
        {
        case ResultCode::RC_Success:
        {
            if (file->GetImage()->GetWidth() <= 16384 && file->GetImage()->GetHeight() <= 16384)
            {
                LoadOivImage(file);
            }
            else
            {
                using namespace std::string_literals;
                SetUserMessage(L"Can not load the file: "s + formattedFilePath + \
                    L", image dimensions are more than 16384: ", static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
            }
        }

            break;
        case ResultCode::RC_FileNotSupported:
            SetUserMessage(L"Can not load the file: "s + formattedFilePath + L", image format is not supported"s, static_cast<GroupID>(UserMessageGroups::FailedFileLoad),MessageFlags::Persistent);
            break;
        default:
            SetUserMessage(L"Can not load the file: "s + formattedFilePath + L", unkown error"s, static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
        }
        return result == RC_Success;

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
            fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), oivImage->GetImageSource() == ImageSource::File);
            fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), oivImage->GetImageSource() == ImageSource::File);
        }

        fRefreshOperation.End();
        fFileDisplayTimer.Stop();

        LoadSubImages();

        fImageState.GetOpenedImage()->SetDisplayTime(fFileDisplayTimer.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds));

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
        fSequencerTimer.SetInterval(fImageState.GetOpenedImage()->GetImage()->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame ? 1 : 0);
    }


    void TestApp::UpdateOpenImageUI()
    {
        if (IsImageOpen())
        {
            UpdateTitle();
            fVirtualStatusBar.SetText("imageDescription", fImageState.GetImage(ImageChainStage::Deformed)->GetDescription());
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
        return fImageState.GetOpenedImage() != nullptr && fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File;
    }

    void TestApp::UpdateOpenedFileIndex()
    {
        if (IsOpenedImageIsAFile())
        {
            LLUtils::ListWStringIterator it = std::find(fListFiles.begin(), fListFiles.end(), GetOpenedFileName());

            if (it != fListFiles.end())
                fCurrentFileIndex = std::distance(fListFiles.begin(), it);
        }
    }



    void TestApp::SortFileList()
    {
        std::sort(fListFiles.begin(), fListFiles.end(), fFileSorter);
    }

    void TestApp::LoadFileInFolder(std::wstring absoluteFilePath)
    {
        using namespace std::filesystem;
        
        const std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();

        if (absoluteFolderPath != fListedFolder)
        {
            auto fileList = GetSupportedFileListInFolder(absoluteFolderPath);

            //File is loaded from a different folder then the active one.
            std::swap(fListFiles, fileList);
            fCurrentFileIndex = FileIndexStart;
            fListedFolder = absoluteFolderPath;
        }
        
        UpdateOpenedFileIndex();
    }

    void TestApp::OnScroll(const LLUtils::PointF64& panAmount)
    {
        Pan(panAmount);
        
        Win32::MainWindow::CursorType cursorType = Win32::MainWindow::CursorType::SystemDefault;

        if (panAmount == LLUtils::PointF64::Zero)
        {
            cursorType = Win32::MainWindow::CursorType::SizeAll;
        }
        else
        {
            double deg = LLUtils::Math::ToDegrees(atan2(-panAmount.y, panAmount.x)) + 180.0;
            const int numDirections = 8;
            const int step = 360 / numDirections;
            int index = (static_cast<int>(deg) + step / 2) % 360 / step;
            cursorType = static_cast<Win32::MainWindow::CursorType>(index + 2);
        }
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
        
        
        future <bool> asyncResult;
        
        if (isInitialFileExists == true)
        {
            fIsTryToLoadInitialFile = true;
                 
            // if initial file is provided, load asynchronously.
            asyncResult = async(launch::async, [&]() ->bool
                {
                    fInitialFile = std::make_shared<OIVFileImage>(filePath);
                    return fInitialFile->Load(&fImageLoader, IMCodec::PluginTraverseMode::NoTraverse) == RC_Success;
                }
            );
        }
        
        // initialize the windowing system of the window
        fWindow.Create();
        fWindow.SetMenuChar(false);
        fWindow.ShowStatusBar(false);
        fWindow.SetDestoryOnClose(false);
        fWindow.EnableDragAndDrop(true);
		// Set canvas background the same color as in the renderer for flicker free startup.
		//TODO: fix resize and disable background erasure of top level windows.
		fWindow.SetBackgroundColor(LLUtils::Color(45, 45, 48));
		fWindow.GetCanvasWindow().SetBackgroundColor(LLUtils::Color(45, 45, 48));

        fWindow.SetDoubleClickMode(::Win32::DoubleClickMode::Default);
        {
            using namespace ::OIV::Win32;
            fWindow.SetWindowStyles(::Win32::WindowStyle::ResizableBorder | ::Win32::WindowStyle::MaximizeButton | ::Win32::WindowStyle::MinimizeButton, true);
        }
    
        AutoScroll::CreateParams params = { fWindow.GetHandle(),Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL, std::bind(&TestApp::OnScroll, this, std::placeholders::_1) };
        fAutoScroll = std::make_unique<AutoScroll>(params);


        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));
        fWindow.GetCanvasWindow().AddEventListener(std::bind(&TestApp::HandleClientWindowMessages, this, _1));


        fRefreshOperation.Begin();

        fTimerNoActiveZoom.SetTargetWindow(fWindow.GetHandle());
        
        fTimerNoActiveZoom.SetCallback(std::bind(&TestApp::DelayResamplingCallback, this));

        fTimerNavigation.SetTargetWindow(fWindow.GetHandle());
         fTimerNavigation.SetCallback([this]()
            {
                 using namespace Win32;
                 using namespace LInput;
                 const auto& mouseState = fMouseDevicesState.begin()->second;
                 const int jump = (mouseState.GetButtonState(static_cast<MouseButtonType>( MouseButton::Forward)) == ButtonState::Down) ? 1 : (mouseState.GetButtonState(static_cast<MouseButtonType>(MouseButton::Back)) == ButtonState::Down) ? -1 : 0;


                 if (jump != 0 && fLastImageLoadTimeStamp.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds) > fQuickBrowseDelay)
                 {
                     
                     fLastImageLoadTimeStamp.Start();
                     fLastImageLoadTimeStamp.Stop();
                     
                     if (JumpFiles(jump) == false)
                     {
                         fLastImageLoadTimeStamp.Start();
                     }
                 }
                
            }
        );

         //TODO: move sequencer initialiaztion to PostInitOperations.
         fSequencerTimer.SetTargetWindow(fWindow.GetHandle());
         fSequencerTimer.SetCallback([this]()
             {
                 auto currentImage = fImageState.GetOpenedImage()->GetImage()->GetSubImage(fCurrentFrame);
                 fImageState.SetImageChainRoot(std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, currentImage));
                 auto nextFrame = (fCurrentFrame + 1) % fImageState.GetOpenedImage()->GetImage()->GetNumSubImages();

                 // When Animation data is not found, set minimum frame delay to 5 milliseconds.
                 constexpr uint32_t MinFrameDelay = 5u;
                 fSequencerTimer.SetInterval(std::max(1u, 
                     static_cast<uint32_t>(static_cast<double>( std::max(MinFrameDelay,currentImage->GetAnimationData().delayMilliseconds)) / fCurrentSequencerSpeed)));
                 fCurrentFrame = nextFrame;
                 RefreshImage();
             });


         fMessageManager = std::make_unique<MessageManager>(fWindow.GetHandle(), &fLabelManager, 5, [&]()->void
         {
                 fRefreshOperation.Queue();
         });
        
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

        // If there is no initial file or the file has failed to load, show the window now, otherwise show the window after 
        // the image has rendered completely at the method FinalizeImageLoad.
        fWindow.SetVisible(!isInitialFileLoadedSuccesfuly);
        
        //If initial file is provided but doesn't exist
        if (isInitialFileProvided && !isInitialFileExists)
        {
            using namespace  std::string_literals;
            SetUserMessage(L"Can not load the file: "s + filePath + L", it doesn't exist"s, static_cast<GroupID>(UserMessageGroups::FailedFileLoad), MessageFlags::Persistent);
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

        const auto isMainAnActualImage = openedImage->GetItemType() != ImageItemType::Container;
        

        if (index == 0 && isMainAnActualImage)
        {
            return openedImage;
        }
        else
        {
            const auto actualIndex = isMainAnActualImage == true ? std::max(0, index - 1) : index;
            return openedImage->GetSubImage(actualIndex);
        }
    }

    void TestApp::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        auto imageIndex = ImageSelectionChangeArgs.imageIndex;
        if (imageIndex  >= 0)
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
        if (fileName == GetOpenedFileName())
        {
            const bool internally = fRequestedFileForRemoval == GetOpenedFileName();

            bool shouldRemoveFile = (internally == true &&  (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedInternally) == DeletedFileRemovalMode::DeletedInternally)
                || (internally == false && (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedExternally) == DeletedFileRemovalMode::DeletedExternally);

            // Don't remove file, just update index
            if (shouldRemoveFile == false)
            {
                if (fCurrentFileIndex > 0)
                    fCurrentFileIndex--;
                else if (fCurrentFileIndex == 0 && fListFiles.size() > 1)
                    fCurrentFileIndex++;
            }
            else
            {
                // Remove and unload the file
                bool isFileLoaded = false;
                if (fListFiles.size() == 1)
                {
                    fCurrentFileIndex = FileIndexStart;
                    isFileLoaded = JumpFiles(FileIndexStart);
                }
                else
                {
                    fCurrentFileIndex -= 1;
                    isFileLoaded = JumpFiles(1);
                    if (isFileLoaded == false)
                    {
                        fCurrentFileIndex += 1;
                        isFileLoaded = JumpFiles(-1);
                    }
                }

                if (isFileLoaded == false)
                {
                    // Could find a suitable file to load, unload current file and reset index
                    UnloadOpenedImaged();
                    ShowWelcomeMessage();
                    fCurrentFileIndex = FileIndexStart;
                }
            }

            fRequestedFileForRemoval = {};
        }
        else
        {
            // File has been added to the current folder, indices have changed - update current file index
            auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), GetOpenedFileName(), fFileSorter);
            fCurrentFileIndex = std::distance(fListFiles.begin(), itCurrentFile);
        }
        UpdateTitle();
    }

    void TestApp::UpdateFileList(FileWatcher::FileChangedOp fileOp, const std::wstring& filePath, const std::wstring& filePath2)
    {
        switch (fileOp)
        {
        case FileWatcher::FileChangedOp::Add:
        {
            //Add file to list only if it's a known file type
            std::wstring extension = LLUtils::StringUtility::ToLower(std::filesystem::path(filePath).extension().wstring());
            std::wstring_view sv(extension);
            if (sv.empty() == false)
                sv = sv.substr(1);

            if (fKnownFileTypesSet.contains(sv.data()))
            {
                //TODO: add file sorted 
                auto itAddedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath, fFileSorter);

                if (itAddedFile != fListFiles.end() && *itAddedFile == filePath)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Trying to add an existing file");

                fListFiles.insert(itAddedFile, filePath);
                
                // File has been added to the current folder, indices have changed - update current file index
                auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), GetOpenedFileName(), fFileSorter);
   	            fCurrentFileIndex = std::distance(fListFiles.begin(), itCurrentFile);
 
                UpdateTitle();
            }
        }
        break;

        case FileWatcher::FileChangedOp::Remove:
        {
            auto it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
            if (it != fListFiles.end())
            {
                auto fileNameToRemove = *it;
                fListFiles.erase(it);
                ProcessRemovalOfOpenedFile(fileNameToRemove);
            }
        }
        break;
        case FileWatcher::FileChangedOp::Rename:
        {
            auto it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
            if (it != fListFiles.end())
            {
                auto fileNameToRemove = *it;
                fListFiles.erase(it);
                auto itRenamedFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath2, fFileSorter);
                fListFiles.insert(itRenamedFile, filePath2);

                if (filePath == GetOpenedFileName())
                {
                    UnloadOpenedImaged();
                    LoadFile(filePath2, IMCodec::PluginTraverseMode::NoTraverse);
                }
                else
                {
                    // File has been added to the current folder, indices have changed - update current file index
                    auto itCurrentFile = std::lower_bound(fListFiles.begin(), fListFiles.end(), GetOpenedFileName(), fFileSorter);
                    fCurrentFileIndex = std::distance(fListFiles.begin(), itCurrentFile);
                    UpdateTitle();
                }

            }
            else
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Invalid file removal request");
            }
        }


          break;

        case FileWatcher::FileChangedOp::Modified:
        case FileWatcher::FileChangedOp::None:
        case FileWatcher::FileChangedOp::WatchedFolderRemoved:
            break;
        }
    }

    void TestApp::OnFileChangedImpl(FileWatcher::FileChangedEventArgs* fileChangedEventArgsPtr)
    {
        auto fileChangedEventArgs = *fileChangedEventArgsPtr;
		
        if (fileChangedEventArgs.folderID == fOpenedFileFolderID)
        {
            std::wstring absoluteFilePath = std::filesystem::path(GetOpenedFileName());
            std::wstring absoluteFolderPath = std::filesystem::path(GetOpenedFileName()).parent_path();
            std::wstring changedFileName = (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName).wstring();
            std::wstring changedFileName2 = (std::filesystem::path(fileChangedEventArgs.folder) / fileChangedEventArgs.fileName2).wstring();

            switch (fileChangedEventArgs.fileOp)
            {
            case FileWatcher::FileChangedOp::None:
                break;
            case FileWatcher::FileChangedOp::Add:
                UpdateFileList(fileChangedEventArgs.fileOp, changedFileName, std::wstring());
                break;
            case FileWatcher::FileChangedOp::Remove:
                UpdateFileList(fileChangedEventArgs.fileOp, changedFileName, std::wstring());
                break;
            case FileWatcher::FileChangedOp::Modified:
                if (absoluteFilePath == changedFileName)
                    ProcessCurrentFileChanged();
                break;
            case FileWatcher::FileChangedOp::Rename:
                UpdateFileList(FileWatcher::FileChangedOp::Rename, changedFileName, changedFileName2);
                if (absoluteFilePath == changedFileName2)
                    ProcessCurrentFileChanged();
                break;

            case FileWatcher::FileChangedOp::WatchedFolderRemoved:
                fCurrentFolderWatched.clear();
                break;
            }
        }
        else if (fileChangedEventArgs.folderID == fCOnfigurationFolderID)
        {
            if (fileChangedEventArgs.fileName == L"Settings.json")
            {
                LoadSettings();
            }
        }
        else
        {
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    void TestApp::OnFileChanged(FileWatcher::FileChangedEventArgs fileChangedEventArgs)
    {
        SendMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WM_NOTIFY_FILE_CHANGED, reinterpret_cast<WPARAM>(&fileChangedEventArgs),0);
    }

    void TestApp::OnMouseEvent(const LInput::ButtonStdExtension<MouseButtonType>::ButtonEvent& btnEvent)
    {
        using namespace LInput;
        bool isMouseCursorOnTopOfWindowAndInsideClientRect = fWindow.IsUnderMouseCursor() && fWindow.IsMouseCursorInClientRect();
        if (btnEvent.button == MouseButton::Middle && btnEvent.eventType == EventType::Pressed
            && isMouseCursorOnTopOfWindowAndInsideClientRect)
        {
            fAutoScroll->ToggleAutoScroll();
            if (fAutoScroll->IsAutoScrolling() == false)
            {
                fWindow.SetCursorType(::OIV::Win32::MainWindow::CursorType::SystemDefault);
                fAutoScrollAnchor.reset();
            }
            else
            {
                std::wstring anchorPath = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) + L"./Resources/Cursors/arrow-C.cur";
                std::unique_ptr<OIVFileImage> fileImage = std::make_unique<OIVFileImage>(anchorPath);
                if (fileImage->Load(&fImageLoader, IMCodec::PluginTraverseMode::AnyPlugin) == RC_Success)
                {
                    fileImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
                    fileImage->SetPosition(static_cast<LLUtils::PointF64>(static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) - static_cast<LLUtils::PointI32>(fileImage->GetImage()->GetDimensions()) / 2));
                    fileImage->SetScale({ 1.0,1.0 });
                    fileImage->SetOpacity(0.5);
                    fileImage->SetVisible(true);
                    fAutoScrollAnchor = std::move(fileImage);
                    //TODO: do we need update here when loading the cursor anchor ? 
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
        const bool IsRightCatured = fCapturedMouseButtons.at(static_cast<size_t>(MouseButtonType::Right)) == true;

        if (btnEvent.button == MouseButton::Left)
        {
            if (btnEvent.eventType == EventType::Pressed && IsRightDown && isMouseCursorOnTopOfWindowAndInsideClientRect)
            {
                // Rocker gesture - navigate backward
                fRockerGestureActivate = true;
                fContextMenuTimer.SetInterval(0);
                JumpFiles(-1);
            }
            else if (IsRightDown == false && IsRightCatured == false)
            {
                //Window drag and resize
                if (Win32Helper::IsKeyPressed(VK_MENU) == false
                    && fWindow.IsFullScreen() == false
                    )
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
                if (fCapturedMouseButtons.at(static_cast<size_t>(MouseButton::Back)) == false
                    && fCapturedMouseButtons.at(static_cast<size_t>(MouseButton::Forward)) == false)
                        fTimerNavigation.SetInterval(0);
            }
                
        }

            

        if (btnEvent.button == MouseButton::Right && btnEvent.eventType == EventType::Pressed && isMouseCursorOnTopOfWindowAndInsideClientRect)
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
        using namespace  LInput;
        using namespace ::Win32;


        const auto& mouseState = fMouseDevicesState.find(mouseInput.deviceIndex)->second;

        //const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
        const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;

        const bool IsRightCatured = fCapturedMouseButtons.at(static_cast<size_t>(MouseButtonType::Right)) == true;
        const bool IsLeftCaptured = fCapturedMouseButtons.at(static_cast<size_t>(MouseButtonType::Left)) == true;
        //const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
       // const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::Released;


        const bool isMouseUnderCursor = fWindow.IsUnderMouseCursor();




        //Quick browse feature
        //const bool isNavigationBackwardDown = (mouseState.GetButtonState(MouseButtonType::Back) == ButtonState::Down);
        //const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseButtonType::Back) == ButtonState::Up);
        //const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseState::Button::Third) == MouseState::State::Up);
        //const bool isNavigationForwardDown = mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Down;
        //const bool isNavigationForwardUp = mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Up;


            //Selection rect
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
            //Browse files
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
                //20% percent zoom in each wheel step
                if (IsRightCatured)
                    //  Zoom to center of the client area if currently panning.
                    Zoom(wheelDelta * 0.2);
                else
                    Zoom(wheelDelta * 0.2, mousePos.x, mousePos.y);
            }
        }


        if (IsRightDown)
        {

            LLUtils::PointI32  currentPosition = Win32Helper::GetMouseCursorPosition();
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
            //if mouse ID not found add new buttonstates entry.
            if (it == std::end(fMouseDevicesState))
            {
                it = fMouseDevicesState.emplace(evnt.deviceIndex, decltype(fMouseDevicesState)::mapped_type()).first;
                //Add standard extension
                auto stdExtension = std::make_shared<ButtonStdExtension<MouseButtonType>>(evnt.deviceIndex, 250, 0);
                stdExtension->OnButtonEvent.Add(std::bind(&TestApp::OnMouseEvent,this, std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(stdExtension));

                
              
                
                //Add multitap extension for click, double click and triple click
                /*
                auto multitapextension = std::make_shared<MultitapExtension<MouseButtonType>>(evnt.deviceIndex, 500, 2);
                multitapextension->OnButtonEvent.Add(std::bind(&TestApp::OnMouseMultiTap, this,std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(multitapextension));
                */
            }

            for (size_t i = 0; i < RawInput::MaxMouseButtons; i++)
            {
                it->second.SetButtonState(static_cast<decltype(fMouseDevicesState)::mapped_type::underlying_button_type>(i), mouseEvent.buttonState[i]);
            
                if (mouseEvent.buttonState[i] == ButtonState::Down && fWindow.IsUnderMouseCursor())
                    fCapturedMouseButtons[static_cast<size_t>(i)] = true;
                else if (mouseEvent.buttonState[i] == ButtonState::Up)
                    fCapturedMouseButtons[static_cast<size_t>(i)] = false;

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
            WatchCurrentFolder();
        }
    }
    void TestApp::PostInitOperations()
    {
        LLUtils::Logger::GetSingleton().AddLogTarget(&mLogFile);



        fTimerTopMostRetention.SetTargetWindow(fWindow.GetHandle());
        fTimerTopMostRetention.SetCallback([this]()
            {
                ProcessTopMost();
            }
        );


        fTimerSlideShow.SetTargetWindow(fWindow.GetHandle());
        fTimerSlideShow.SetCallback([this]()
            {
                SetSlideShowEnabled(false);

                bool foundFile = JumpFiles(1) ||
                    ((fCurrentFileIndex == std::distance(fListFiles.begin(), fListFiles.end()) - 1) && JumpFiles(FileIndexStart));

                SetSlideShowEnabled(foundFile);
            });

        fDoubleTap.callback = [this]()
        {
            fWindow.SetAlwaysOnTop(true);
            fTopMostCounter = 3;
            SetTopMostUserMesage();
            fTimerTopMostRetention.SetInterval(1000);
        };


        auto codecsInfo = fImageLoader.GetImageCodec().GetPluginsInfo();

        //Build known image extension set and open/save dialog filters
        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters readFilters;
        ::Win32::FileDialogFilterBuilder::ListFileDialogFilters writeFilters;

        readFilters.push_back({ L"All files (*.*)" , {{ L"*.*" }} });

        readFilters.push_back({ L"All supported image formats" , {} });

        auto allFormatsIdx = readFilters.size() - 1;
        

        for (const auto& codecInfo : codecsInfo)
        {
            for (const auto& extensionCollection : codecInfo.extensionCollection)
            {
                
                    if ((codecInfo.capabilities & IMCodec::CodecCapabilities::Decode) == IMCodec::CodecCapabilities::Decode)  // Can the codec decode data
                    {
                        //Prepare read filters for Open file Dialog, and Prepare known file types data structure
                        readFilters.push_back({});
                        auto& readFilter = readFilters.back();

                        std::wstring readDialogDescription;
                        for (const auto& extension : extensionCollection.listExtensions)
                        {
                            readDialogDescription += LLUtils::StringUtility::ToUpper(extension) + L'/';
                            auto lowercaseExtension = LLUtils::StringUtility::ToLower(extension);
                            fKnownFileTypesSet.insert(lowercaseExtension);
                            readFilter.extensions.push_back(L"*." + lowercaseExtension);
                            readFilters.at(allFormatsIdx).extensions.push_back(L"*." + lowercaseExtension);
                        }

                        if (readDialogDescription.empty() == false)
                            readDialogDescription.erase(readDialogDescription.length() - 1, 1);


                        readFilter.description = readDialogDescription + L" - " + extensionCollection.description;
                    }

                    if ( (codecInfo.capabilities & IMCodec::CodecCapabilities::Encode) == IMCodec::CodecCapabilities::Encode
                        &&  (codecInfo.capabilities & IMCodec::CodecCapabilities::BulkCodec) != IMCodec::CodecCapabilities::BulkCodec) // Make sure it isn't a bulk codec
                        
                    {
                        //Prepare write filter for Save file Dialog 
                        writeFilters.push_back({});
                        auto& writeFilter = writeFilters.back();

                        std::wstring saveDialogDescription;
                        for (const auto& extension : extensionCollection.listExtensions)
                        {
                            saveDialogDescription += LLUtils::StringUtility::ToUpper(extension) + L'/';
                            auto lowercaseExtension = LLUtils::StringUtility::ToLower(extension);
                            if (writeFilter.extensions.empty()) // Add only the primary format.
                                writeFilter.extensions.push_back(L"*." + lowercaseExtension);

                            if (lowercaseExtension == L"png")
                                fDefaultSaveFileFormatIndex = static_cast<int16_t>(writeFilters.size());

                        }

                        if (saveDialogDescription.empty() == false)
                            saveDialogDescription.erase(saveDialogDescription.length() - 1, 1);


                        writeFilter.description = saveDialogDescription + L" - " + extensionCollection.description;
                    }
                
            }
        }

        if (fDefaultSaveFileFormatIndex == -1)
            fDefaultSaveFileFormatIndex = 0;

        fOpenComDlgFilters = { readFilters };
        fSaveComDlgFilters = { writeFilters };

        std::wstringstream ss;

        for (const auto& knownExtension : fKnownFileTypesSet)
            ss << knownExtension << L';';
        
        if (ss.rdbuf()->in_avail() > 0)
        {
            ss.seekp(-1, std::ios_base::end);
            ss << L'\0';
        }
        
        fKnownFileTypes = ss.str();

        fFileWatcher.FileChangedEvent.Add(std::bind(&TestApp::OnFileChanged, this, std::placeholders::_1));

        //If a file has been succesfuly loaded, index all the file in the folder
        ProcessLoadedDirectory();
        UpdateTitle();

        AddCommandsAndKeyBindings();

        fWindow.GetImageControl().GetImageList().ImageSelectionChanged.Add(std::bind(&TestApp::OnImageSelectionChanged, this, std::placeholders::_1));
		
		// renderer took over on the window, no need to erase background.
		fWindow.GetCanvasWindow().SetEraseBackground(false);

        fContextMenuTimer.SetTargetWindow(fWindow.GetHandle());
        fContextMenuTimer.SetCallback(std::bind(&TestApp::OnContextMenuTimer, this));
        fContextMenu = std::make_unique<ContextMenu<MenuItemData>>(fWindow.GetHandle());
    	
        fContextMenu->AddItem(LLUTILS_TEXT("Open"), MenuItemData{ "cmd_open_file","" });
        fContextMenu->AddItem(LLUTILS_TEXT("Open containing folder"), MenuItemData{ "cmd_shell","cmd=containingFolder" });
    	fContextMenu->AddItem(LLUTILS_TEXT("Open in new window"), MenuItemData{ "cmd_shell","cmd=newWindow" });
        fContextMenu->AddItem(LLUTILS_TEXT("Open in photoshop"), MenuItemData{ "cmd_shell","cmd=openPhotoshop" });
        fContextMenu->AddItem(LLUTILS_TEXT("Quit"), MenuItemData{ "cmd_view_state","type=quit" });


        fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), fImageState.GetOpenedImage() != nullptr && fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File);
        fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), fImageState.GetOpenedImage() != nullptr && fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File);

              

    	
        fNotificationIconID = fNotificationIcons.AddIcon(MAKEINTRESOURCE(IDI_APP_ICON), LLUTILS_TEXT("Open Image Viewer"));
        fNotificationIcons.OnNotificationIconEvent.Add(std::bind(&TestApp::OnNotificationIcon, this, std::placeholders::_1));

        fNotificationContextMenu = std::make_unique < ContextMenu<int>> (fWindow.GetHandle());
        fNotificationContextMenu->AddItem(OIV_TEXT("Quit"), int{});

        using namespace LInput;
        fRawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls, RawInput::GenericDesktopControlsUsagePage::Mouse, RawInput::Flags::EnableBackground);

        fRawInput.OnInput.Add(std::bind(&TestApp::OnRawInput, this,std::placeholders::_1));
        fRawInput.Enable(true);

        fMouseClickEventHandler.OnMouseClickEvent.Add(std::bind(&TestApp::OnMouseMultiClick, this, std::placeholders::_1));

        LoadSettings();

        if (fReloadSettingsFileIfChanged)
            fCOnfigurationFolderID = fFileWatcher.AddFolder(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/."));


        if (fPendingFolderLoad.empty() == false)
        {
            LoadFileOrFolder(fPendingFolderLoad, IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType);
            fPendingFolderLoad.clear();
        }

        if (IsImageOpen() == false)
        {
            ShowWelcomeMessage();
            UpdateTitle();
        }

        //Register clipboard format by PRIORITY
        fClipboardHelper.RegisterFormat(CF_DIBV5);
        fClipboardHelper.RegisterFormat(CF_DIB);
        /*fHTMLFormatID = fClipboardHelper.RegisterFormat(L"HTML Format");
        fRTFFormatID = fClipboardHelper.RegisterFormat(L"Rich Text Format");*/
        fClipboardHelper.RegisterFormat(CF_UNICODETEXT);
        fClipboardHelper.RegisterFormat(CF_TEXT);
    }

    template <typename value_type>
    value_type ParseValue(const std::wstring& value)
    {
        using Integral = ConfigurationLoader::Integral;
        using Float = ConfigurationLoader::Float;
        using Bool = ConfigurationLoader::Bool;

        if constexpr (std::is_same_v< value_type, Integral>)
            return std::stoll(value);
        else if constexpr (std::is_same_v< value_type, Float>)
            return std::stod(value);
        else if constexpr (std::is_same_v< value_type, Bool >)
            return value == L"true" ? true : false;
        else
            static_assert("not implemented");
    }
    
    void TestApp::OnSettingChange(const std::wstring& key, const std::wstring& value)
    {
        using Integral = ConfigurationLoader::Integral;
        using Float = ConfigurationLoader::Float;
        using Bool = ConfigurationLoader::Bool;
        
        if (key == L"viewsettings/maxzoom")
        {
            auto val = ParseValue<Float>(value);
            fMaxPixelSize = val;
        }
        else if (key == L"viewsettings/imagemargins/x")
            fImageMargins.x = ParseValue<Float>(value);
        else if (key == L"viewsettings/imagemargins/y")
            fImageMargins.y = ParseValue<Float>(value);
        else if (key == L"viewsettings/minimagesize")
            fMinImageSize = ParseValue<Float>(value);
        else if (key == L"viewsettings/slideshowinterval")
            fSlideShowIntervalms = static_cast<uint32_t>(ParseValue<Integral>(value));
        else if (key == L"viewsettings/quickbrowsedelay")
            fQuickBrowseDelay = static_cast<uint16_t>(ParseValue<Integral>(value));

        //Auto scroll

        else if (key == L"autoscroll/deadzoneradius")
            fAutoScroll->SetDeadZoneRadius(static_cast<int32_t>(ParseValue<Integral>(value)));
        else if (key == L"autoscroll/speedfactorin")
            fAutoScroll->SetSpeedFactorIn(ParseValue<Float>(value));
        else if (key == L"autoscroll/speedfactorout")
            fAutoScroll->SetSpeedFactorOut(ParseValue<Float>(value));
        else if (key == L"autoscroll/speedfactorrange")
            fAutoScroll->SetSpeedFactorRange(static_cast<int32_t>(ParseValue<Integral>(value)));
        else if (key == L"autoscroll/maxspeed")
            fAutoScroll->SetMaxSpeed(static_cast<int32_t>(ParseValue<Integral>(value)));

        //deleted file removal mode

        else if (key == L"filesystem/deletedfileremovalmode")
        {
            std::wstring fileRemovalModeStr = value;
            
            if (fileRemovalModeStr == L"always")
                fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedExternally | DeletedFileRemovalMode::DeletedInternally;
            else if (fileRemovalModeStr == L"externally")
                fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedExternally;
            else if (fileRemovalModeStr == L"internally")
                fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedInternally;
            else if (fileRemovalModeStr == L"none")
                fDeletedFileRemovalMode = DeletedFileRemovalMode::None;
        }
        // modified file reload mode
        else if (key == L"filesystem/modifiedfilereloadmode")
        {
            std::wstring modifiedFileReloadModeStr = value;
            if (modifiedFileReloadModeStr == L"none")
                fMofifiedFileReloadMode = MofifiedFileReloadMode::None;
            else if (modifiedFileReloadModeStr == L"confirmation")
                fMofifiedFileReloadMode = MofifiedFileReloadMode::Confirmation;
            else if (modifiedFileReloadModeStr == L"autoforeground")
                fMofifiedFileReloadMode = MofifiedFileReloadMode::AutoForeground;
            else if (modifiedFileReloadModeStr == L"autobackground")
                fMofifiedFileReloadMode = MofifiedFileReloadMode::AutoBackground;
        }
        else if (key == L"system/reloadsettingsfileifchanged")
        {
            fReloadSettingsFileIfChanged = ParseValue<Bool>(value);
        }
        else if (key == L"files/defaultsortmode")
        {
            if (value == L"name")
                fFileSorter.SetSortType(FileSorter::SortType::Name);
            else if (value == L"date")
                fFileSorter.SetSortType(FileSorter::SortType::Date);
            else if (value == L"extension")
                fFileSorter.SetSortType(FileSorter::SortType::Extension);
        }
        else if (key == L"files/sortbynamedirection")
            fFileSorter.SetSortDirection(FileSorter::SortType::Name, value == L"ascending" ? FileSorter::SortDirection::Ascending : FileSorter::SortDirection::Descending);
        else if (key == L"files/sortbydatedirection")
            fFileSorter.SetSortDirection(FileSorter::SortType::Date, value == L"ascending" ? FileSorter::SortDirection::Ascending : FileSorter::SortDirection::Descending);
        else if (key == L"files/sortbyextensiondirection")
            fFileSorter.SetSortDirection(FileSorter::SortType::Extension, value == L"ascending" ? FileSorter::SortDirection::Ascending : FileSorter::SortDirection::Descending);
        

        else if (key == L"displaysettings/backgroundcolor1")
        {
            //auto argb = ;
            //LLUtils::Color backgroundColor1 = { argb.channels[0], argb.channels[1] ,argb.channels[2] , argb.channels[3] };
            ApiGlobal::sPictureRenderer->SetBackgroundColor(0, LLUtils::Color::FromString(LLUtils::StringUtility::ToAString(value)));
            fRefreshOperation.Queue();
        }
        else if (key == L"displaysettings/backgroundcolor2")
        {
            /*auto argb = ;
            LLUtils::Color backgroundColor2 = { argb.channels[1], argb.channels[2] ,argb.channels[3] , argb.channels[0] };*/
            ApiGlobal::sPictureRenderer->SetBackgroundColor(1, LLUtils::Color::FromString(LLUtils::StringUtility::ToAString(value)));
            fRefreshOperation.Queue();
        }
        else if (key == L"imagesettings/biggestsubimage")
        {
            fDisplayBiggestSubImageOnLoad = ParseValue<Bool>(value);
        }
        
    }

    void TestApp::LoadSettings()
    {
        auto settings = ConfigurationLoader::LoadSettings();
        for (const auto& pair : settings)
            OnSettingChange(LLUtils::StringUtility::ToWString(pair.first), LLUtils::StringUtility::ToWString(pair.second));
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
                auto bottomLeft = rect.GetCorner(LLUtils::Corner::TopRight);
                bottomLeft.x += rect.GetWidth() / 2;
                bottomLeft.y += rect.GetHeight() / 2;

                fWindow.SetForground();
                auto chosenItem = fNotificationContextMenu->Show(bottomLeft.x, bottomLeft.y, AlignmentHorizontal::Right, AlignmentVertical::Bottom);
                if (chosenItem != nullptr)
                {
                    using namespace std::string_literals;
                    CommandRequestIntenal request;
                    request.commandName = "cmd_view_state";
                    request.args = "type="s + LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ConvertString<decltype(request.commandName)>(chosenItem->itemDisplayName));
                    ExecuteCommandInternal(request);
                }
             }
            break;
            case NotificationIconGroup::NotificationIconAction::None:
                LL_EXCEPTION_UNEXPECTED_VALUE;
            break;
        }
    }



    double TestApp::PerformColorOp(double& gamma, const std::string& op, const std::string& val)
    {
        double value = std::atof(val.c_str());
        if (op == "increase")
            gamma *= (1 + value / 100.0);
        else if (op == "decrease")
            gamma *= 1 / (1 + value / 100.0);
        else if (op == "add")
            gamma += value;
        else if (op == "subtract")
            gamma -=  value;
        return gamma;
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
        if (memcmp(&fColorExposure, &DefaultColorCorrection,sizeof(OIV_CMD_ColorExposure_Request)) == 0)
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

    void TestApp::Run()
    {
        ::Win32::Win32Helper::MessageLoop();
    }


    bool TestApp::JumpFiles(FileIndexType step)
    {
        if (fListFiles.empty())
            return false;

        FileCountType totalFiles = fListFiles.size();
        FileIndexType fileIndex = fCurrentFileIndex;


        int sign;
        if (step == FileIndexEnd)
        {
            // Last
            fileIndex = static_cast<int32_t>(fListFiles.size());
            sign = -1;
        }
        else if (step == FileIndexStart)
        {
            // first
            fileIndex = -1;
            sign = 1;
        }
        else
        {
            sign = step > 0 ? 1 : -1;
        }

        bool isLoaded = false;
        LLUtils::ListWStringIterator it;

        do
        {
            fileIndex += sign;
            
            if (fileIndex < 0 || fileIndex >= static_cast<FileIndexType>(totalFiles) || fileIndex == fCurrentFileIndex)
                break;
            
            it = fListFiles.begin();
            std::advance(it, fileIndex);
        }
        
        while ((isLoaded = LoadFile(*it, IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::OnlyKnownFileType)) == false);


        if (isLoaded)
        {
            assert(fileIndex >= 0 && fileIndex < static_cast<FileIndexType>(totalFiles));
            fCurrentFileIndex = fileIndex;
        }
        return isLoaded;
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
            fWindow.SetWindowStyles(WindowStyle::ResizableBorder | WindowStyle::MaximizeButton | WindowStyle::MinimizeButton, fShowBorders);
        }
        
    }


    void TestApp::SetSlideShowEnabled(bool enabled)
    {
        if (fSlideShowEnabled != enabled)
        {
            fSlideShowEnabled = enabled;
            if (fSlideShowEnabled == true)
            {
                fTimerSlideShow.SetInterval(fSlideShowIntervalms);
            }
            else
            {
                fTimerSlideShow.SetInterval(0);
            }
        }
    }
    

    void TestApp::SetFilterLevel(OIV_Filter_type filterType)
    {
        fImageState.GetVisibleImage()->SetFilterType(std::clamp(filterType
            , FT_None, static_cast<OIV_Filter_type>( FT_Count - 1)));

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
        LInput::KeyCombination keyCombination = LInput::KeyCombination::FromVirtualKey(static_cast<uint32_t>(evnt->message.wParam),
            static_cast<uint32_t>(evnt->message.lParam));
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

    void TestApp::Pan(const LLUtils::PointF64& panAmount )
    {
        if (fImageState.GetOpenedImage() != nullptr)
            SetOffset(panAmount * fDPIadjustmentFactor + fImageState.GetOffset());
    }

    void TestApp::Zoom(double amount, int zoomX , int zoomY )
    {
        if (IsImageOpen())
        {
            CommandManager::CommandRequest request;
            request.displayName = "Zoom";
            request.args = CommandManager::CommandArgs::FromString("val=" + std::to_string(amount) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY));
            request.commandName = "cmd_zoom";
            ExecuteCommand(request);
        }
    }

    void TestApp::ZoomInternal(double amount, int zoomX, int zoomY)
    {
        const double adaptiveAmount = fAdaptiveZoom.Add(amount);
        const double adjustedAmount = adaptiveAmount > 0 ? GetScale() * (1 + adaptiveAmount) : GetScale() / (1 - adaptiveAmount);
        SetZoomInternal(adjustedAmount, zoomX, zoomY);
    }
    
    void TestApp::FitToClientAreaAndCenter()
    {
        if (IsImageOpen())
        {
            using namespace LLUtils;
            SIZE clientSize = fWindow.GetCanvasSize();
            if (clientSize.cx > 0 && clientSize.cy > 0) // window might minimized.
            {
                PointF64 ratio = PointF64(clientSize.cx, clientSize.cy) / GetImageSize(ImageSizeType::Transformed);
                double zoom = std::min(ratio.x, ratio.y);
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
            return  fImageState.GetImage(ImageChainStage::SourceImage) != nullptr ? PointF64(fImageState.GetImage(ImageChainStage::SourceImage)->GetImage()->GetDimensions()) : PointF64(0, 0);
        case ImageSizeType::Transformed:
            return static_cast<PointF64>(fImageState.GetImage(ImageChainStage::Deformed)->GetImage()->GetDimensions());
        case ImageSizeType::Visible:
            return fImageState.GetVisibleSize();

        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    void TestApp::UpdateSelectionRectText()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        auto selectionSizeStr = std::to_wstring(fImageSpaceSelection.GetWidth()) + L" X " + std::to_wstring(fImageSpaceSelection.GetHeight());

        if (selectionSizeText == nullptr)
        {
            // Create new user message.
            selectionSizeText = fLabelManager.GetOrCreateTextLabel("selectionSizeText");
            selectionSizeText->SetFontPath(LabelManager::sFontPath);
            selectionSizeText->SetFontSize(11);
            selectionSizeText->SetBackgroundColor({ 0,0,0, 192});
            selectionSizeText->SetTextColor({ 170, 170, 170, 255 });
            selectionSizeText->SetTextRenderMode(FreeType::RenderMode::Antialiased);
            selectionSizeText->SetOutlineWidth(0);

            selectionSizeText->SetFilterType(OIV_Filter_type::FT_None);
            selectionSizeText->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
            selectionSizeText->SetScale({ 1.0,1.0 });
            selectionSizeText->SetOpacity(1.0);
        }

        auto selectionRectPosition = fSelectionRect.GetSelectionRect().GetCorner(LLUtils::Corner::TopLeft);
        // text already exists, just make visible.
        selectionSizeText->SetVisible(true);
        selectionSizeText->SetText(selectionSizeStr);
        selectionSizeText->Create();
       
        int32_t posX = selectionRectPosition.x + fSelectionRect.GetSelectionRect().GetWidth() / 2  - selectionSizeText->GetImage()->GetWidth() / 2;
        int32_t posY = selectionRectPosition.y - selectionSizeText->GetImage()->GetHeight();

        const auto selectinTopLeft = fSelectionRect.GetSelectionRect().GetCorner(LLUtils::Corner::TopLeft);
        const auto selectinBottomRight = fSelectionRect.GetSelectionRect().GetCorner(LLUtils::Corner::BottomRight);
        const auto clientSize = static_cast<LLUtils::PointI32>(fWindow.GetClientSize());


        //if vetical position is above client area, place text below selection rect     
        if (posY < 0)
            posY = selectinBottomRight.y;

        // if vertical position is below client area, place text inside the rectangle
        if (posY + static_cast<int32_t>(selectionSizeText->GetImage()->GetHeight()) >= clientSize.y)
            posY =  std::max(0, selectionRectPosition.y);

        // if horizontal position is far right

        if (posX + static_cast<int32_t>(selectionSizeText->GetImage()->GetWidth()) >= clientSize.x)
        {
            posX = selectionRectPosition.x - selectionSizeText->GetImage()->GetWidth();
            posY = selectinTopLeft.y + (selectinBottomRight.y - selectinTopLeft.y) / 2;
        }

        if (posX < 0)
        {
            posX = selectinBottomRight.x;
            posY = selectinTopLeft.y + (selectinBottomRight.y - selectinTopLeft.y) / 2;
        }


        selectionSizeText->SetPosition({ static_cast<double>(posX), static_cast<double>(posY) });
    }

    void TestApp::OnImageReady(IMCodec::ImageSharedPtr image)
    {
    }

    LLUtils::PointI32 TestApp::SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen)
    {
        using namespace LLUtils;
        auto imageSpacePoint = static_cast<LLUtils::PointI32>(ClientToImage(pointOnScreen).Round());
        auto snappedscreenSpacePoint = ImageToClient(static_cast<LLUtils::PointF64>(imageSpacePoint));
        return  static_cast<PointI32>(snappedscreenSpacePoint.Round());
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
        if (fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
            SetImageSpaceSelection(ClientToImageRounded(fSelectionRect.GetSelectionRect()));
    }

    void TestApp::LoadImageSpaceSelection()
    { 
        if (fImageSpaceSelection.IsEmpty() == false)
        {
            LLUtils::RectI32 r = static_cast<LLUtils::RectI32>(ImageToClient(static_cast<LLUtils::RectF64>(fImageSpaceSelection)));
            fSelectionRect.UpdateSelection(r);
            UpdateSelectionRectText();
        }
    }

    void TestApp::CancelSelection()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        if (selectionSizeText != nullptr)
            selectionSizeText->SetVisible(false);

        fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, { 0,0 });
        fImageSpaceSelection = decltype(fImageSpaceSelection)::Zero;
    }

    double TestApp::GetMinimumPixelSize()
    {
        using namespace LLUtils;
        PointF64 minimumZoom = fMinImageSize / GetImageSize(ImageSizeType::Transformed);
        return std::min(std::max(minimumZoom.x, minimumZoom.y), 1.0);
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

        //Apply zoom limits only if zoom is not bound to the client window
        if (fIsLockFitToScreen == false)
        {
            //We want to keep the image at least the size of 'MinImagePixelsInSmallAxis' pixels in the smallest axis.
            zoomValue = std::clamp(zoomValue, GetMinimumPixelSize(), fMaxPixelSize);
        }

        if (zoomValue != fImageState.GetScale().x)
        {
            //Save image selection before view change
            fPreserveImageSpaceSelection.Begin();
            
            PointI32 clientZoomPoint = { clientX, clientY };

            if (clientZoomPoint.x < 0 || clientZoomPoint.y < 0)
            {
                const PointI32 canvasCenter = static_cast<PointI32>(GetCanvasCenter());
                if (clientZoomPoint.x < 0)
                    clientZoomPoint.x = canvasCenter.x;

                if (clientZoomPoint.y < 0)
                    clientZoomPoint.y = canvasCenter.y;
            }

            PointF64 imageZoomPoint = ClientToImage(clientZoomPoint);
            PointF64 offset = (imageZoomPoint / GetImageSize(ImageSizeType::Original)) * (GetScale() - zoomValue) * GetImageSize(ImageSizeType::Original);

            QueueResampling();

            fImageState.SetScale(zoomValue);

            fRefreshOperation.Begin();

            RefreshImage();

            // preserve offset lock (image centering) if zoom is realtive to the center of the image
            SetOffset(GetOffset() + offset, clientX == -1 && clientY == -1);
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
        using namespace LLUtils;
        return imagepos * GetScale() + GetOffset();
    }

    LLUtils::RectF64 TestApp::ImageToClient(LLUtils::RectF64 clientRect) const
    {
        using namespace LLUtils;
        return {
              ImageToClient(clientRect.GetCorner(Corner::TopLeft))
            , ImageToClient(clientRect.GetCorner(Corner::BottomRight))
        };
    }


    LLUtils::PointF64 TestApp::ClientToImage(LLUtils::PointI32 clientPos) const
    {
        using namespace LLUtils;
        return (static_cast<PointF64>(clientPos) - GetOffset()) / GetScale();
    }

    LLUtils::RectF64 TestApp::ClientToImage(LLUtils::RectI32 clientRect) const
    {
        using namespace LLUtils;
        return {
              ClientToImage(clientRect.GetCorner(Corner::TopLeft))
            , ClientToImage(clientRect.GetCorner(Corner::BottomRight))
        };
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
                ss << L"Texel: "
                    << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.x
                    << L" X "
                    << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.y;
                fVirtualStatusBar.SetText("texelPos", ss.str());

                //fWindow.SetStatusBarText(ss.str(), 2, 0);

                PointF64 storageImageSize = GetImageSize(ImageSizeType::Transformed);

                
                if (!(storageImageSpace.x < 0
                    || storageImageSpace.y < 0
                    || storageImageSpace.x >= storageImageSize.x
                    || storageImageSpace.y >= storageImageSize.y
                    ))
                {
                    std::wstring message = StringUtility::ConvertString<OIVString>(OIVHelper::ParseTexelValue(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(),static_cast<LLUtils::PointI32>(storageImageSpace)));
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
        if (fIsLockFitToScreen == true && fIsOffsetLocked)
            FitToClientAreaAndCenter();
        else if (fIsOffsetLocked == true || forceCenter == true)
            Center();
        fRefreshOperation.End();
    }

    void TestApp::UpdateWindowSize()
    {
        SIZE size = fWindow.GetCanvasSize();

        if (size.cx > 0 && size.cy > 0) // window might minimized.
        {
            CmdSetClientSizeRequest req{ static_cast<uint16_t>(size.cx),
                static_cast<uint16_t>(size.cy) };

            OIVCommands::ExecuteCommand(CMD_SetClientSize,
                &req, &NullCommand);
            //UpdateCanvasSize();
            AutoPlaceImage();
            auto point = static_cast<LLUtils::PointI32>(fWindow.GetCanvasSize());
            fVirtualStatusBar.ClientSizeChanged(point);

            EventManager::GetSingleton().SizeChange.Raise(EventManager::SizeChangeEventParams{ static_cast<int32_t>(size.cx) , static_cast<int32_t>(size.cy)} );
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
            RECT primaryMonitorCoords = ::Win32::MonitorInfo::GetSingleton().GetPrimaryMonitor(false).monitorInfo.rcMonitor;
            RECT boundingArea = ::Win32::MonitorInfo::GetSingleton().getBoundingMonitorArea();

            using point_type = PointF64::point_type;
            auto leftDelta = primaryMonitorCoords.left - boundingArea.left;
            auto topDelta = primaryMonitorCoords.top - boundingArea.top;

            const LLUtils::PointF64 primaryScreenOffset = LLUtils::PointF64(static_cast<point_type>(leftDelta)
                , static_cast<point_type>(topDelta));

            const LLUtils::PointF64 primaryScreenSize = LLUtils::PointF64(static_cast<point_type>(primaryMonitorCoords.right - primaryMonitorCoords.left)
                , static_cast<point_type>(primaryMonitorCoords.bottom - primaryMonitorCoords.top));

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
            PointF64 offset = GetCanvasCenter() - GetImageSize(ImageSizeType::Visible) / 2;
            //Lock offset when centering
            fIsOffsetLocked = true;
            SetOffset(offset, true);
			fRefreshOperation.End();
		}
    }

    double CalculateOffset(double clientSize, double imageSize, double offset, double margin)
    {
        double fixedOffset = offset;
        if (imageSize > clientSize)
        {
            if (offset > 0)
                fixedOffset = std::min<double>(clientSize * margin, offset);


            else if (offset < 0)
                fixedOffset = std::max<double>(-imageSize + (clientSize  * ( 1- margin) ), offset);

        }
        else
        {
            if (offset < 0)
            {
                fixedOffset = std::max<double>(-imageSize *  margin, offset);
            }

            else if (offset > 0)
            {
                fixedOffset = std::min<double>(clientSize - imageSize * (1 - margin) , offset);
            }
        }
        return fixedOffset;
    }

    LLUtils::PointF64 TestApp::ResolveOffset(const LLUtils::PointF64& point)
    {
        using namespace LLUtils;
        PointF64 imageSize = GetImageSize(ImageSizeType::Visible);
        PointF64 clientSize = fWindow.GetCanvasSize();
        PointF64 offset = static_cast<PointF64>(point);
        
        offset.x = CalculateOffset(clientSize.x, imageSize.x, offset.x, fImageMargins.x);
        offset.y = CalculateOffset(clientSize.y, imageSize.y, offset.y, fImageMargins.y);
        return offset;
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

    void TestApp::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, IMCodec::TexelFormat texelFormat)
    {
        std::shared_ptr<OIVRawImage>  rawImage = std::make_shared<OIVRawImage>(ImageSource::Clipboard);
        RawBufferParams params;
        params.width = width;
        params.height = height;
        params.rowPitch = rowPitch;
        params.texelFormat = texelFormat;
        params.buffer = buffer;
       //TODO: uncouple vertical flip from 'LoadRaw'
        ResultCode result = rawImage->Load(params, { IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical });

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

            const std::byte* bitmapBitsconst =  reinterpret_cast<const std::byte*>(info) + info->biSize;
            std::byte* bitmapBits = const_cast<std::byte*>(bitmapBitsconst);

            switch (info->biCompression)
            {
            case BI_RGB:
                break;
            case BI_BITFIELDS:
                bitmapBits += 3 * sizeof(DWORD);
                break;
            default:
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, std::string("Unsupported clipboard bitmap compression type :") + std::to_string(info->biCompression));
            }

            using namespace IMCodec;
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
            ImageDescriptor& props = imageItem->descriptor;

            imageItem->itemType = ImageItemType::Image;
            props.height = info->biHeight;
            props.width = info->biWidth;
            props.texelFormatStorage = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8 : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.texelFormatDecompressed = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8 : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.rowPitchInBytes = rowPitch;
            const size_t bufferSize = props.rowPitchInBytes * props.height;
            imageItem->data.Allocate(bufferSize);
            imageItem->data.Write(bitmapBits, 0, bufferSize);
            auto image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
            
            if (info->biCompression == BI_BITFIELDS) // no support for alpha channel, convert to BGR
                image = IMUtil::ImageUtil::Convert(image,  IMCodec::TexelFormat::I_B8_G8_R8);

            image = IMUtil::ImageUtil::Transform({ IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical }, image);

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
                text = (wchar_t*)buffer.data();
            else if (formatType == CF_TEXT)
                text = LLUtils::StringUtility::ToWString((const char*)buffer.data());


            if (text.empty() == false)
            {
                OIVTextImageSharedPtr textImage = std::make_shared<OIVTextImage>(ImageSource::ClipboardText, fFreeType.get());
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
                textImage->SetTextColor({ 48, 48, 48, 255 });
                textImage->SetUseMetaText(false);
                //text->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
                textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
                //textImage->Create();
                textImage->Create();
                //textImage->GetImage();
                LoadOivImage(textImage);
                clipboardType = ClipboardDataType::Text;
            }
        }
        return clipboardType;
    }

    bool TestApp::SetClipboardImage(IMCodec::ImageSharedPtr image)
    {
        auto clipboardCompatibleImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8, false);
        if (clipboardCompatibleImage != nullptr)
        {
            uint32_t width = clipboardCompatibleImage->GetWidth();
            uint32_t height = clipboardCompatibleImage->GetHeight();
            uint8_t bpp = clipboardCompatibleImage->GetBitsPerTexel();
            auto dibBUffer = LLUtils::PlatformUtility::CreateDIB<1>(width, height, bpp, clipboardCompatibleImage->GetRowPitchInBytes(), clipboardCompatibleImage->GetBuffer());
            auto result =  fClipboardHelper.SetClipboardData(CF_DIB, dibBUffer);

            if (result == ::Win32::ClipboardResult::Success)
            {
                auto dibV5BUffer = LLUtils::PlatformUtility::CreateDIB<5>(width, height, bpp, clipboardCompatibleImage->GetRowPitchInBytes(), clipboardCompatibleImage->GetBuffer());
                result = fClipboardHelper.SetClipboardData(CF_DIBV5, dibV5BUffer);
            }

            if (result == ::Win32::ClipboardResult::Success)
                return true;
        }
        return false;
    }

    OperationResult TestApp::CopyVisibleToClipBoard()
    {
        OperationResult result = OperationResult::UnkownError;
        if (IsImageOpen())
        {
            if (fSelectionRect.GetSelectionRect().IsEmpty())
            {
                result = OperationResult::NoSelection;
            }
            else
            {
                LLUtils::RectI32 imageSpaceSelection = ClientToImageRounded(fSelectionRect.GetSelectionRect());
                auto cropped = IMUtil::ImageUtil::CropImage(fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(), imageSpaceSelection);

                if (cropped != nullptr)
                {
                    //2. Flip the image vertically and convert it to BGRA for the clipboard.
                    auto flipped = IMUtil::ImageUtil::Transform({ IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical }, cropped);
                    if (flipped != nullptr && SetClipboardImage(flipped))
                        result = OperationResult::Success;
                }
            }
        }
        else
        {
            result = OperationResult::NoDataFound;
        }
        return result;
    }

    OperationResult TestApp::CropVisibleImage()
    {
        OperationResult result = OperationResult::UnkownError;
        if (IsImageOpen() == false)
        {
            result = OperationResult::NoDataFound;
        }
        else
        {
            if (fSelectionRect.GetSelectionRect().IsEmpty())
            {
                result = OperationResult::NoSelection;
            }
            else
            {

                LLUtils::RectI32 imageRectInt = ClientToImageRounded(fSelectionRect.GetSelectionRect());
                auto cropped = IMUtil::ImageUtil::CropImage(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(), imageRectInt);

                if (cropped != nullptr)
                {
                    auto oivCropped = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, cropped);
                    LoadOivImage(oivCropped);
                    CancelSelection();
                    result = OperationResult::Success;
                }
            }
        }
        return result;
    }

    OperationResult TestApp::CutSelectedArea()
    {
        OperationResult result = OperationResult::UnkownError;
        //Please note that currently this function works on the rasterized image, a more general solution is needed to work on a previous stage image.
        if (IsImageOpen() == false)
        {
            result = OperationResult::NoDataFound;
        }
        else
        {
            auto rasterized = fImageState.GetImage(ImageChainStage::Rasterized)->GetImage();
            if (fSelectionRect.GetSelectionRect().IsEmpty())
            {
                result = OperationResult::NoSelection;
            }
            else
            {
                LLUtils::RectI32 subImageRect = ClientToImageRounded(fSelectionRect.GetSelectionRect());

                const LLUtils::RectI32 imageRect = { { 0,0 } ,{ static_cast<int32_t> (rasterized->GetWidth())
              , static_cast<int32_t> (rasterized->GetHeight()) } };

                subImageRect = subImageRect.Intersection(imageRect);

                if (subImageRect.IsEmpty() == false)
                {
                    SetClipboardImage(IMUtil::ImageUtil::GetSubImage(rasterized, subImageRect));
                    auto& texelInfo = IMCodec::GetTexelInfo(fImageState.GetImage(ImageChainStage::Rasterized)->GetImage()->GetOriginalTexelFormat());
                    bool hasOpacityChannel = false;
                    for (auto& channel : texelInfo.channles)
                        if (channel.semantic == IMCodec::ChannelSemantic::Opacity)
                        {
                            hasOpacityChannel = true;
                            break;
                        }
                    
                    const auto fillColor = hasOpacityChannel ? LLUtils::Color(0, 0, 0, 0) : LLUtils::Color(0, 0, 0, 255);
                    auto colorFilled = IMUtil::ImageUtil::FillColor(fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(), subImageRect, fillColor);

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

        const WinMessage & message = evnt->message;

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
        std::wstring message = L"Top most ending in..." + std::to_wstring(fTopMostCounter);
        SetUserMessage(message, static_cast<GroupID>(UserMessageGroups::WindowOnTop) ,MessageFlags::Interchangeable | MessageFlags::ManualRemove);
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
            if (fIsActive == true && fPendingReloadFileName.empty() == false && fPendingReloadFileName == GetOpenedFileName())
            {
                PerformReloadFile(fPendingReloadFileName);
                fPendingReloadFileName = {};
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
        if (fPendingReloadFileName == requestedFile)
        {
            if (fMofifiedFileReloadMode != MofifiedFileReloadMode::Confirmation)
            {
                LoadFile(requestedFile, IMCodec::PluginTraverseMode::NoTraverse);
            }
            else
            {
                using namespace std::string_literals;
                int mbResult = MessageBox(fWindow.GetHandle(), (L"Reload the file: "s + requestedFile).c_str(), L"File is changed outside of OIV", MB_YESNO);
                switch (mbResult)
                {
                case IDYES:
                    LoadFile(GetOpenedFileName(), IMCodec::PluginTraverseMode::NoTraverse);
                    break;
                case IDNO:
                    break;
                }
            }
        }
    }
    

    void TestApp::ProcessCurrentFileChanged()
    {
        switch (fMofifiedFileReloadMode)
        {
        case MofifiedFileReloadMode::AutoBackground:
            LoadFile(GetOpenedFileName(), IMCodec::PluginTraverseMode::NoTraverse); // Load file immediatly
            break;
        case MofifiedFileReloadMode::AutoForeground:
        case MofifiedFileReloadMode::Confirmation: // implicitly foreground
            if (GetAppActive())
                PerformReloadFile(GetOpenedFileName());
            else 
                fPendingReloadFileName = GetOpenedFileName();
            break;
        case MofifiedFileReloadMode::None: // do nothing
            break;

        }
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
        case Win32::UserMessage::PRIVATE_WM_COUNT_COLORS:
        {
            fIsColorThreadRunning = false;

            if (fImageState.GetImage(ImageChainStage::SourceImage).get() == reinterpret_cast<OIVBaseImage*>(uMsg.wParam))
            {
                // Still the same image on display, assing number of colors and refresh ImageInfo

                // if counting unique colors has failed, assign UniqueColorsFailed, so counting colors won't restart for this image.
                fCountingImageColor.reset();
                fImageState.GetImage(ImageChainStage::SourceImage)->SetNumUniqueColors((int64_t)uMsg.lParam != UniqueColorsUninitialized -1 ? (int64_t)uMsg.lParam : UniqueColorsFailed);

                if (GetImageInfoVisible() == true)
                    ShowImageInfo();
            }
            else
            {
                // If a different image on display Just count colors
                if (GetImageInfoVisible() == true)
                    CountColorsAsync();
            }
        }
        break;
            case WM_COPYDATA:
            {
                COPYDATASTRUCT* cds = (COPYDATASTRUCT*)uMsg.lParam;
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
            KeyCombination keyCombination = KeyCombination::FromVirtualKey(static_cast<uint32_t>(evnt->message.wParam),
                static_cast<uint32_t>(evnt->message.lParam));

            bool isAltup = (keyCombination.keydata().keycode == KeyCode::LALT || keyCombination.keydata().keycode == KeyCode::RIGHTALT || keyCombination.keydata().keycode == KeyCode::RALT);

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
        HANDLE mutex = CreateMutex(NULL, FALSE,  (LLUtils::native_string_type(Globals::ProgramGuid) + LLUTILS_TEXT("_CLOSEAPP")).c_str() );
        if (mutex == nullptr)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be created.");
        }

        const DWORD result = WaitForSingleObject(
            mutex,    // handle to mutex
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

    LLUtils::ListWString TestApp::GetSupportedFileListInFolder(const std::wstring& folderPath)
    {
        LLUtils::ListWString fileList;
        if (std::filesystem::is_directory(folderPath))
        {
            LLUtils::FileSystemHelper::FindFiles(fileList, folderPath, fKnownFileTypes, false, false);
            std::sort(fileList.begin(), fileList.end(), fFileSorter);
        }
        else
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Not a folder");
        }
       
        return fileList;
    }



    bool TestApp::LoadFileOrFolder(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode)
    {

        bool success = false;
        if (std::filesystem::is_directory(filePath))
        {
         
            auto fileList = GetSupportedFileListInFolder(filePath);
            size_t i;
            std::shared_ptr<OIVFileImage> file;
            ResultCode result = ResultCode::RC_NotInitialized;

            if (fileList.empty() == false)
            {
                // Traverse file list untill a file has been successfully loaded
                for (i = 0; i < fileList.size(); i++)
                {
                    file = std::make_shared<OIVFileImage>(fileList.at(i));
                    result = file->Load(&fImageLoader, IMCodec::PluginTraverseMode::NoTraverse);
                    if (result == RC_Success)
                        break;

                }
            }

            if (result == RC_Success)
            {
                std::swap(fListFiles, fileList);
                fCurrentFileIndex = i;
                fListedFolder = filePath;
                LoadOivImage(file);
                success = true;
            }
        }

        else
        {
            if (LoadFile(filePath, traverseMode))
                success = true;
        }

     

        return success;
    }
	
    bool TestApp::HandleFileDragDropEvent(const ::Win32::EventDdragDropFile* event_ddrag_drop_file)
    {

        std::wstring normalizedPath = std::filesystem::path(event_ddrag_drop_file->fileName).lexically_normal().wstring();
        if (LoadFileOrFolder(normalizedPath, IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
        {
            fWindow.SetForground();
            return true;
        }

        return false;

    }

    bool TestApp::ExecutePredefinedCommand(std::string command)
    {
        CommandManager::CommandRequest commandRequest = fCommandManager.GetCommandRequestGroup(command);
        return  commandRequest.commandName.empty() == false && ExecuteCommand(commandRequest);
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
        CommandManager::CommandRequest request{ "",requestInternal.commandName,CommandManager::CommandArgs::FromString(requestInternal.args) };
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
        //Ensure shared tr refcount doesn't get to zero 
        // by assiging it to a private memeber field.

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
                fCountingColorsThread = std::thread([](OIVBaseImageSharedPtr image, HWND windowHandle)-> void
                    {
                        int64_t uniqueValues = PixelHelper::CountUniqueValues(image->GetImage());
                ::PostMessage(windowHandle, Win32::UserMessage::PRIVATE_WM_COUNT_COLORS, (WPARAM)image.get(), (LPARAM)uniqueValues);
                

                    }, fCountingImageColor, fWindow.GetHandle());
            }
        }
    }

    void TestApp::ShowImageInfo()
    {
        if (IsImageOpen())
        {
            CountColorsAsync();

            std::wstring imageInfoString = MessageHelper::CreateImageInfoMessage(
                fImageState.GetOpenedImage(), 
                fImageState.GetImage(ImageChainStage::SourceImage)
            , fImageLoader.GetImageCodec() );
            OIVTextImage* imageInfoText = fLabelManager.GetOrCreateTextLabel("imageInfo");

            imageInfoText->SetText(imageInfoString);
            imageInfoText->SetBackgroundColor(LLUtils::Color(0, 0, 0, 127));
            imageInfoText->SetFontPath(LabelManager::sFixedFontPath);
            imageInfoText->SetFontSize(12);
            //imageInfoText->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
            imageInfoText->SetOutlineWidth(2);
            imageInfoText->SetPosition({ 20,60 });

            if (imageInfoText->IsDirty())
                fRefreshOperation.Queue();
        }
    }


    void TestApp::ShowWelcomeMessage()
    {
        using namespace std;

        string message = "<textcolor=#4a80e2>Welcome to <textcolor=#dd0f1d>OIV\n"\
            "<textcolor=#25bc25>Drag <textcolor=#4a80e2>here an image to start\n"\
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
        //get the text size to reposition on screen
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

}
 
