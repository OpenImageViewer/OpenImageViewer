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


#include "OIVImage\OIVHandleImage.h"
#include "OIVImage\OIVFileImage.h"
#include "OIVImage\OIVRawImage.h"
#include "Helpers\OIVImageHelper.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "globals.h"
#include "ConfigurationLoader.h"


#include "resource.h"

namespace OIV
{
    void TestApp::CMD_Zoom(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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
            SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(GetFilterType()) + 1));
            filterTypeChanged = true;
        }
        else if (type == "imageFilterDown")
        {
            SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(GetFilterType()) - 1));
            filterTypeChanged = true;
        }
        else if (type == "toggleFullScreen") // Toggle fullscreen
        {
			ToggleFullScreen(false);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleMultiFullScreen") //Toggle multi fullscreen
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

    void TestApp::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request, [[maybe_unused]] CommandManager::CommandResult& result)
    {
        auto type = request.args.GetArgValue("type");
        if (type == "imageinfo") // Toggle image info
        {
            SetImageInfoVisible(!GetImageInfoVisible());
        }
        else // Toggle keybindings
        {
            OIVTextImage* text = fLabelManager.GetTextLabel("keyBindings");
            if (text != nullptr) //
            {
                fLabelManager.Remove("keyBindings");
                fRefreshOperation.Queue();
                return;
            }

            text = fLabelManager.GetOrCreateTextLabel("keyBindings");
            CreateTextParams& requestText = text->GetTextOptions();
            
            auto message = MessageHelper::CreateKeyBindingsMessage();

            requestText.text = LLUtils::StringUtility::ConvertString<OIVString>(message);
            requestText.backgroundColor = LLUtils::Color(0, 0, 0, 216).colorValue;
            requestText.fontPath = LabelManager::sFixedFontPath;
            requestText.fontSize = 12;
            requestText.renderMode = OIV_PROP_CreateText_Mode::CTM_AntiAliased;
            requestText.outlineWidth = 2;


            OIV_CMD_ImageProperties_Request& imageProperties = text->GetImageProperties();
            imageProperties.position = { 20,60 };
            imageProperties.filterType = OIV_Filter_type::FT_None;
            //imageProperties.imageHandle = responseText.imageHandle;
            imageProperties.imageRenderMode = IRM_Overlay;
            imageProperties.scale = 1.0;
            imageProperties.opacity = 1.0;
            imageProperties.visible = true;

            if (text->Update() == RC_Success)
                fRefreshOperation.Queue();

        }
    }

    void TestApp::CMD_OpenFile([[maybe_unused]]  const CommandManager::CommandRequest& request, [[maybe_unused]] CommandManager::CommandResult& response)
    {
        std::wstring fileName = ::Win32::Win32Helper::OpenFile(fWindow.GetHandle());
        if (fileName.empty() == false)
        {
            LoadFile(fileName, false);
        }
    }

    void TestApp::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request, CommandManager::CommandResult& response)
    {
        OIV_AxisAlignedRotation rotation = OIV_AxisAlignedRotation::AAT_None;
        OIV_AxisAlignedFlip flip = OIV_AxisAlignedFlip::AAF_None;

        std::string type = request.args.GetArgValue("type");

        if (false);
        else if (type == "hflip")
            flip = OIV_AxisAlignedFlip::AAF_Horizontal;
        else if (type == "vflip")
            flip = OIV_AxisAlignedFlip::AAF_Vertical;
        else if (type == "rotatecw")
            rotation = AAT_Rotate90CW;
        else if (type == "rotateccw")
            rotation = AAT_Rotate90CCW;


        if (rotation != OIV_AxisAlignedRotation::AAT_None || flip != OIV_AxisAlignedFlip::AAF_None)
        {
            TransformImage(rotation, flip);

            std::wstring rotation;
            switch (fImageState.GetAxisAlignedRotation())
            {
            case AAT_Rotate90CW:
                rotation = L"90 degrees clockwise";
                break;
            case AAT_Rotate180:
                rotation = L"180 degrees";
                break;
            case AAT_Rotate90CCW:
                rotation = L"180 degrees counter clockwise";
                break;
            case AAT_None:
                break;
            }
            if (rotation.empty() == false)
                response.resValue += std::wstring(L"Rotation <textcolor=#7672ff>(") + rotation +  L')';


            std::wstring flip;

            switch (fImageState.GetAxisAlignedFlip())
            {
            case AAF_Horizontal:
                flip = L"horizontal";
                break;
            case AAF_Vertical:
                flip = L"vertical";
                break;
            case AAF_None:
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
                LLUtils::PlatformUtility::CopyTextToClipBoard(GetOpenedFileName());
                result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
            }
        }
        else if (cmd == "selectedArea")
        {
            CopyVisibleToClipBoard();
            result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        }
    }


    void TestApp::CMD_PasteFromClipboard(const CommandManager::CommandRequest& request,
        CommandManager::CommandResult& result)
    {
        if (PasteFromClipBoard())
            result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
        else
            result.resValue = L"Nothing usable in clipboard";

    }

    void TestApp::CMD_ImageManipulation(const CommandManager::CommandRequest& request,
        CommandManager::CommandResult& result)
    {
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");
        if (cmd == "cropSelectedArea")
        {
            CropVisibleImage();
        }
        else if (cmd == "selectAll")
        {
            result.resValue = LLUtils::StringUtility::ToWString(request.displayName);
            using namespace LLUtils;
            RectI32 imageInScreenSpace = static_cast<LLUtils::RectI32>(ImageToClient({ { 0.0,0.0 }, { GetImageSize(ImageSizeType::Transformed) } }));

            fRefreshOperation.Begin();
            fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, { 0,0 });
            fSelectionRect.SetSelection(SelectionRect::Operation::BeginDrag, imageInScreenSpace.GetCorner(Corner::TopLeft));
            fSelectionRect.SetSelection(SelectionRect::Operation::Drag, imageInScreenSpace.GetCorner(Corner::BottomRight));
            fSelectionRect.SetSelection(SelectionRect::Operation::EndDrag, imageInScreenSpace.GetCorner(Corner::BottomRight));
            SaveImageSpaceSelection();
            fRefreshOperation.End();
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
                imageList.SetSelected(nextIndex);
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
		return GetAppDataFolder() + L"oiv.log";
	}


    void TestApp::HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args)
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
        
        ss << "call stack:" << endl << args.callstack;

		mLogFile.Log(ss.str());
        //MessageBoxW(IsMainThread() ? fWindow.GetHandle() : nullptr, displayMessage.c_str(), L"Unhandled exception has occured.", MB_OK | MB_APPLMODAL);
        //DebugBreak();
    }

    TestApp::TestApp()
        : fRefreshTimer(std::bind(&TestApp::OnRefreshTimer, this))
        , fRefreshOperation(std::bind(&TestApp::OnRefresh, this))
        , fPreserveImageSpaceSelection(std::bind(&TestApp::OnPreserveSelectionRect, this))
        , fSelectionRect(std::bind(&TestApp::OnSelectionRectChanged, this,std::placeholders::_1, std::placeholders::_2))
        , fVirtualStatusBar(&fLabelManager, std::bind(&TestApp::OnLabelRefreshRequest, this))
    {
        EventManager::GetSingleton().MonitorChange.Add(std::bind(&TestApp::OnMonitorChanged, this, std::placeholders::_1));



        DefaultTextKeyColorTag = std::wstring(L"<textcolor=#")
            + IntToHex(DefaultTextKeyColor.colorValue) + L">";

        DefaultTextValueColorTag = std::wstring(L"<textcolor=#")
            + IntToHex(DefaultTextValueColor.colorValue) + L">"; 
                

        OIV_CMD_RegisterCallbacks_Request request;
		

        request.OnException = [](OIV_Exception_Args args, void* userPointer)
        {
            using namespace std;
            //Convert from C to C++
            LLUtils::Exception::EventArgs localArgs;
            localArgs.errorCode = static_cast<LLUtils::Exception::ErrorCode>(args.errorCode);
            localArgs.functionName = args.functionName;
            localArgs.callstack = args.callstack;
            localArgs.description = args.description;
            localArgs.systemErrorMessage = args.systemErrorMessage;
			reinterpret_cast<TestApp*>(userPointer)->HandleException(true, localArgs);
        };
		request.userPointer = this;

        OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &NullCommand);

        LLUtils::Exception::OnException.Add([this](LLUtils::Exception::EventArgs args)
        {
            HandleException(false, args);
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

    }

    void TestApp::OnLabelRefreshRequest()
    {
        fRefreshOperation.Queue();
    }

    void TestApp::OnMonitorChanged(const EventManager::MonitorChangeEventParams& params)
    {
        //update the refresh rate.
        fRefreshRateTimes1000 = params.monitorDesc.DisplaySettings.dmDisplayFrequency == 59 ? 59940 : params.monitorDesc.DisplaySettings.dmDisplayFrequency * 1000;
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

    void TestApp::UpdateTitle()
    {
        auto GetBuildTimeStamp = []()-> std::wstring
        {
            auto fileTime = std::filesystem::last_write_time(LLUtils::PlatformUtility::GetDllPath());
            std::chrono::system_clock::time_point systemTime;
            auto osVersion = LLUtils::PlatformUtility::GetOSVersion();
            if (osVersion.major > 10 || (osVersion.major == 10 && osVersion.build >= 15063 /*Version 1703*/ ))
            {
                // Not sure if it's a MS STL bug, but using clock_cast invokes initialization of timezones information
				// which in turn invokes icu.dll, supported only since windows 10 1703.
                // https://docs.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu-
                systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
            }
            else
            {
                auto ticks = fileTime.time_since_epoch().count() - std::filesystem::__std_fs_file_time_epoch_adjustment;
                systemTime = std::chrono::system_clock::time_point(std::chrono::system_clock::duration(ticks));
            }
            
            auto in_time_t = std::chrono::system_clock::to_time_t(systemTime);
            std::wstringstream ss;
            tm tmDest;
            errno_t errorCode = localtime_s(&tmDest, &in_time_t) != 0;
            if (errorCode != 0)
            {
                using namespace std::string_literals;
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "could not convert time_t to a tm structure, error code: "s + std::to_string(errorCode));
            }
            ss << std::put_time(&tmDest, OIV_TEXT("%Y-%m-%d %X"));
            return ss.str();
        };
    
        const static std::wstring cachedVersionString = OIV_TEXT("OpenImageViewer ") + std::to_wstring(OIV_VERSION_MAJOR) + L'.' + std::to_wstring(OIV_VERSION_MINOR)
        
 // If not official release add revision and build number
#if OIV_OFFICIAL_RELEASE == 0
        + L"." +  LLUtils::StringUtility::ToWString(OIV_VERSION_REVISION) + L"." + std::to_wstring(OIV_VERSION_BUILD)
            + OIV_TEXT(" | ") + GetBuildTimeStamp()
#endif			
        

// If not official build, i.e. from unofficial / unknown source, add an "UNOFFICIAL" remark.
#if OIV_OFFICIAL_BUILD == 0

             + OIV_TEXT(" | UNOFFICIAL")
#endif
			;

		std::wstring title;
        if (GetOpenedFileName().empty() == false)
        {
            std::wstringstream ss;
            ss << L"File " << (fCurrentFileIndex == FileIndexStart ?
                0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size() << " | ";
            
            title = ss.str() + GetOpenedFileName() + L" - ";
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
        std::unique_ptr<wchar_t> buffer = std::unique_ptr<wchar_t>(new wchar_t[stringLength + 2]);

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

        int shResult = SHFileOperation(&file_op);

        if (shResult != 0 )
        {
                //handle error
        }
        


    }

    void TestApp::RefreshImage()
    {
        fRefreshOperation.Begin();
        fImageState.Refresh();
        fRefreshOperation.End(true);
        UpdateOpenImageUI();
    }

    void TestApp::DisplayOpenedFileName()
    {
        if (IsOpenedImageIsAFile())
            SetUserMessage(L"File: " + MessageFormatter::FormatFilePath(GetOpenedFileName()));
    }

    void TestApp::FinalizeImageLoad(ResultCode result)
    {
        if (result == RC_Success)
        {
            // Enter this function only from the main thread.
            assert("TestApp::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());
            
            fRefreshOperation.Begin();
			
            fImageState.ResetUserState();

            if (fResetTransformationMode == ResetTransformationMode::ResetAll)
                FitToClientAreaAndCenter();
            
            AutoPlaceImage();

            if (fIsTryToLoadInitialFile == false)
                UpdateTitle();

            fImageState.Refresh(); // Make sure a render compatible image is available.
            fWindow.SetShowImageControl(fImageState.GetOpenedImage()->GetDescriptor().NumSubImages > 0);
            UnloadWelcomeMessage();
            DisplayOpenedFileName();

            if (fIsTryToLoadInitialFile == false)
            {
                fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), true);
                fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), true);
            }
        	
            fRefreshOperation.End();
            fFileDisplayTimer.Stop();
            const_cast<ImageDescriptor&>(fImageState.GetOpenedImage()->GetDescriptor()).DisplayTime = fFileDisplayTimer.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

            fLastImageLoadTimeStamp.Start(); 
        }

        if (fIsTryToLoadInitialFile == true)
        {
            fWindow.SetVisible(true);
            fIsTryToLoadInitialFile = false;
        }
        else
        {
            SetResamplingEnabled(true);
            LoadFileInFolder(GetOpenedFileName());
            WatchCurrentFolder();
        }


        if (fQueueImageInfoLoad == true)
        {
            SetImageInfoVisible(true);
            fQueueImageInfoLoad = false;
        }
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

    void TestApp::AddImageToControl(OIVBaseImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        OIV_CMD_GetPixels_Request pixelsRequest;
        OIV_CMD_GetPixels_Response pixelsResponse;

        OIVBaseImageSharedPtr bgraImage = OIVImageHelper::ConvertImage(image, TF_I_B8_G8_R8_A8, false);


        ImageHandle windowCompatibleBitmapHandle;

        if (OIVCommands::TransformImage(bgraImage->GetDescriptor().ImageHandle, OIV_AxisAlignedRotation::AAT_None, OIV_AxisAlignedFlip::AAF_Vertical, windowCompatibleBitmapHandle) == RC_Success)
        {
            bgraImage = std::make_shared<OIVHandleImage>(windowCompatibleBitmapHandle);
        }


        pixelsRequest.handle = bgraImage->GetDescriptor().ImageHandle;

        if (OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_GetPixels, &pixelsRequest, &pixelsResponse) != RC_Success)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not retrieve pixels");
        

        OIVBaseImageSharedPtr bgrImage = OIVImageHelper::ConvertImage(bgraImage, TF_I_B8_G8_R8, false);
        OIV_CMD_GetPixels_Request pixelsRequestBGR;
        OIV_CMD_GetPixels_Response pixelsResponseBGR;
        pixelsRequestBGR.handle = bgrImage->GetDescriptor().ImageHandle;
        OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_GetPixels, &pixelsRequestBGR, &pixelsResponseBGR);

        uint8_t bpp;
        OIV_Util_GetBPPFromTexelFormat(pixelsResponseBGR.texelFormat, &bpp);


        ::Win32::BitmapBuffer bitmapBuffer{};
        bitmapBuffer.bitsPerPixel = bpp;
        bitmapBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(pixelsResponseBGR.rowPitch, sizeof(DWORD));

        LLUtils::Buffer colorBuffer(pixelsResponseBGR.width * bitmapBuffer.rowPitch);
        bitmapBuffer.buffer = colorBuffer.data();
        bitmapBuffer.height = pixelsResponseBGR.height;
        bitmapBuffer.width = pixelsResponseBGR.width;


        //Create 24 bit mask image.
        ::Win32::BitmapBuffer maskBuffer{};
        maskBuffer.bitsPerPixel = 24;
        maskBuffer.height = pixelsResponse.height;
        maskBuffer.width = pixelsResponse.width;
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
        for (size_t l = 0; l < maskBuffer.height; l++)
        {
            const uint32_t sourceOffset = l * pixelsResponse.rowPitch;
            const uint32_t colorOffset = l * bitmapBuffer.rowPitch;
            const uint32_t maskOffset = l * maskBuffer.rowPitch;

            for (size_t x = 0; x < maskBuffer.width; x++)
            {
                Color24& destMask = reinterpret_cast<Color24*>(reinterpret_cast<uint8_t*>(maskPixelsBuffer.data()) + maskOffset)[x];
                Color24& destImage = reinterpret_cast<Color24*>(reinterpret_cast<uint8_t*>(colorBuffer.data()) + colorOffset)[x];
                const Color32& sourceColor = reinterpret_cast<const Color32*>(reinterpret_cast<const uint8_t*>(pixelsResponse.pixelBuffer) + sourceOffset)[x];

                const uint8_t AlphaChannel = sourceColor.A;
                const uint8_t invAlpha = 0xFF - AlphaChannel;
                destMask.R = AlphaChannel;
                destMask.G = AlphaChannel;
                destMask.B = AlphaChannel;
                
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


    void TestApp::LoadSubImages()
    {
        auto mainImage = fImageState.GetOpenedImage();
        mainImage->FetchSubImages();
        auto subImages = mainImage->GetSubImages();
        
        if (subImages.empty() == false)
        {
            const auto totalImages = subImages .size() + 1;

            AddImageToControl(mainImage, static_cast<uint16_t>(0), static_cast<uint16_t>(totalImages));

            for (size_t i = 0; i < subImages.size(); i++)
            {
                auto& currentSubImage = subImages[i];
                AddImageToControl(currentSubImage, static_cast<uint16_t>(i + 1), static_cast<uint16_t>(totalImages));
            }
            //Reset selected sub image when loading new set of subimages
            fWindow.GetImageControl().GetImageList().SetSelected(-1);
        }
        else
        {
            fWindow.GetImageControl().GetImageList().Clear();
        }
    }

    void TestApp::FinalizeImageLoadThreadSafe(ResultCode result)
    {
        if (IsMainThread())
        {
            FinalizeImageLoad(result);
        }
        else
        {
            // Wait for the main window to get initialized.
            std::unique_lock<std::mutex> ul(fMutexWindowCreation);
            
            // send message to main thread.
            PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_NOTIFY_LOADED, result, 0);
        }
    }

    bool TestApp::LoadFile(std::wstring filePath, bool onlyRegisteredExtension)
    {
        std::wstring normalizedPath = std::filesystem::path(filePath).lexically_normal().wstring();
        std::shared_ptr<OIVFileImage> file = std::make_shared<OIVFileImage>(normalizedPath);
        FileLoadOptions loadOptions;
        loadOptions.onlyRegisteredExtension = onlyRegisteredExtension;
        fFileDisplayTimer.Start();
        ResultCode result = file->Load(loadOptions);
        using namespace  std::string_literals;
        switch (result)
        {
        case ResultCode::RC_Success:
            fQueueImageInfoLoad = GetImageInfoVisible();
            SetImageInfoVisible(false);
            SetResamplingEnabled(false);
            fImageState.SetOpenedImage(file);
            //TODO: Load Sub images after finalizing image for faster experience.
            LoadSubImages();
            FinalizeImageLoadThreadSafe(result);
            break;
        case ResultCode::RC_FileNotSupported:
            SetUserMessageThreadSafe(L"Can not load the file: "s + normalizedPath + L", image format is not supported"s);
            break;
        default:
            SetUserMessageThreadSafe(L"Can not load the file: "s + normalizedPath + L", unkown error"s);
        }

        return result == RC_Success;
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
        return fImageState.GetOpenedImage() != nullptr && fImageState.GetOpenedImage()->GetDescriptor().Source == ImageSource::File;
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

    void TestApp::LoadFileInFolder(std::wstring absoluteFilePath)
    {
        using namespace std::filesystem;
        
        const std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();

        if (absoluteFolderPath != fListedFolder)
        {
            fListFiles.clear();
            fCurrentFileIndex = FileIndexStart;

            std::string fileTypesAnsi;
            OIVCommands::GetKnownFileTypes(fileTypesAnsi);

            std::wstring fileTypes = LLUtils::StringUtility::ToWString(fileTypesAnsi);

            LLUtils::FileSystemHelper::FindFiles(fListFiles, absoluteFolderPath, fileTypes, false, false);

            std::sort(fListFiles.begin(), fListFiles.end(), fFileListSorter);

            UpdateOpenedFileIndex();
            UpdateTitle();
            fListedFolder = absoluteFolderPath;
        }
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

        const bool isInitialFileProvided = filePath.empty() == false;
        const bool isInitialFileExists = isInitialFileProvided && filesystem::exists(filePath);
        
        
        future <bool> asyncResult;
        
        if (isInitialFileExists == true)
        {
            fIsTryToLoadInitialFile = true;
                 
            fMutexWindowCreation.lock();
            // if initial file is provided, load asynchronously.
            asyncResult = async(launch::async, &TestApp::LoadFile, this, filePath, false);
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
            using namespace OIV::Win32;
            fWindow.SetWindowStyles(::Win32::WindowStyle::ResizableBorder | ::Win32::WindowStyle::MaximizeButton | ::Win32::WindowStyle::MinimizeButton, true);
        }
    
        AutoScroll::CreateParams params = { fWindow.GetHandle(),Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL, std::bind(&TestApp::OnScroll, this, std::placeholders::_1) };
        fAutoScroll = std::make_unique<AutoScroll>(params);


        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));
        fWindow.GetCanvasWindow().AddEventListener(std::bind(&TestApp::HandleClientWindowMessages, this, _1));


        if (isInitialFileExists == true)
            fMutexWindowCreation.unlock();
        
		fTimerHideUserMessage.SetTargetWindow(fWindow.GetHandle());
		fTimerHideUserMessage.SetCallback([this]()
			{
				HideUserMessageGradually();
			}
		);

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
            SetUserMessage(L"Can not load the file: "s + filePath + L", it doesn't exist"s);
        }

        fRefreshOperation.End(!isInitialFileLoadedSuccesfuly);
    }

    void TestApp::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        auto imageIndex = ImageSelectionChangeArgs.imageIndex;
        if (imageIndex  >= 0)
        {
            auto image = imageIndex == 0 ? fImageState.GetOpenedImage() : fImageState.GetOpenedImage()->GetSubImages()[imageIndex - 1];
            fImageState.SetImageChainRoot(image);
            fRefreshOperation.Begin();
            RefreshImage();
            FitToClientAreaAndCenter();
            fRefreshOperation.End();
        }
    }


    void TestApp::UpdateFileList(FileWatcher::FileChangedOp fileOp, const std::wstring& filePath)
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
                auto it = std::lower_bound(fListFiles.begin(), fListFiles.end(), filePath, fFileListSorter);

                if (it != fListFiles.end() && *it == filePath)
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Trying to add an existing file");

                fListFiles.insert(it, filePath);

                if (IsImageOpen()  == false)
                {
                    auto it = std::lower_bound(fListFiles.begin(), fListFiles.end(), GetOpenedFileName(), fFileListSorter);
                    fCurrentFileIndex = std::distance(fListFiles.begin(), it);
                }
                UpdateTitle();
            }
        }
        break;

        case FileWatcher::FileChangedOp::Remove:
        {
            auto it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
            if (it != fListFiles.end())
            {
                const bool isOpenedFileDeleted = *it == GetOpenedFileName();
                fListFiles.erase(it);

                bool isFileLoaded = false;
                //If the current open file is removed, try load another one in the folder if not unload it
                if (isOpenedFileDeleted)
                {

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
                }

                if (isFileLoaded == false)
                {
                    
                    if (isOpenedFileDeleted)
                    {
                        //current open file has been deleted and no other file has been loaded instead - so remove it
                        UnloadOpenedImaged();
                        fCurrentFileIndex = FileIndexStart;
                    }
                    else
                    {
                        //other file deleted but no file has been loaded, just update title and fileindex
                        auto it = std::lower_bound(fListFiles.begin(), fListFiles.end(), GetOpenedFileName(), fFileListSorter);
                        fCurrentFileIndex = std::distance(fListFiles.begin(), it);
                    }
                    
                    UpdateTitle();
                }
            }
        }
        break;
        case FileWatcher::FileChangedOp::Modified:
        case FileWatcher::FileChangedOp::None:
        case FileWatcher::FileChangedOp::Rename:
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
                UpdateFileList(fileChangedEventArgs.fileOp, changedFileName);
                break;
            case FileWatcher::FileChangedOp::Remove:
                UpdateFileList(fileChangedEventArgs.fileOp, changedFileName);
                break;
            case FileWatcher::FileChangedOp::Modified:
                if (absoluteFilePath == changedFileName)
                    ProcessCurrentFileChanged();
                break;
            case FileWatcher::FileChangedOp::Rename:
                UpdateFileList(FileWatcher::FileChangedOp::Remove, changedFileName);
                UpdateFileList(FileWatcher::FileChangedOp::Add, changedFileName2);
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
                LoadSettings(false);
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
                fWindow.SetCursorType(OIV::Win32::MainWindow::CursorType::SystemDefault);
                fAutoScrollAnchor.reset();
            }
            else
            {
                std::wstring anchorPath = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) + L"./Resources/Cursors/arrow-C.cur";
                std::unique_ptr< OIVFileImage> fileImage = std::make_unique<OIVFileImage>(anchorPath);
                FileLoadOptions options = {};
                options.onlyRegisteredExtension = true;
                if (fileImage->Load(options) == ResultCode::RC_Success)
                {

                    fileImage->GetImageProperties().imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
                    fileImage->GetImageProperties().position = static_cast<LLUtils::PointF64>(static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) - LLUtils::PointI32(fileImage->GetDescriptor().Width, fileImage->GetDescriptor().Height) / 2);
                    fileImage->GetImageProperties().scale = { 1,1 };
                    fileImage->GetImageProperties().opacity = 0.5;

                    fAutoScrollAnchor = std::move(fileImage);
                    fAutoScrollAnchor->Update();
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
        const bool IsRightCatured = fCapturedMouseButtons.at(static_cast<size_t>(MouseButtonType::Right)) == true;

        if (btnEvent.button == MouseButton::Left)
        {
            if (IsRightDown == false && IsRightCatured == false)
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
        }

        if (btnEvent.button == MouseButton::Left)
        {
            if (Win32Helper::IsKeyPressed(VK_MENU))
            {
                SelectionRect::Operation op = SelectionRect::Operation::NoOp;
                if (btnEvent.eventType == EventType::Pressed && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::BeginDrag;
                else if (btnEvent.eventType == EventType::Released && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::EndDrag;
                fSelectionRect.SetSelection(op, fWindow.GetMousePosition());
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
            if (fContextMenuTimer.GetInterval() == 0)
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
        //const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
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
                fSelectionRect.SetSelection(SelectionRect::Operation::Drag, fWindow.GetMousePosition());
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
            if (args.button == MouseButton::Left)
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

            if (args.button == MouseButton::Right)
            {
                ExecutePredefinedCommand("PasteImageFromClipboard");
            }
        }
    }
    void TestApp::PostInitOperations()
    {

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

        fFileWatcher.FileChangedEvent.Add(std::bind(&TestApp::OnFileChanged, this, std::placeholders::_1));

        //If a file has been succesfuly loaded, index all the file in the folder
		if (IsOpenedImageIsAFile())
		{
			LoadFileInFolder(GetOpenedFileName());
            WatchCurrentFolder();
		}
		else
		{
			ShowWelcomeMessage();
			UpdateTitle();
		}

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


        fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), fImageState.GetOpenedImage() != nullptr);
        fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), fImageState.GetOpenedImage() != nullptr);

        std::string fileTypesAnsi;
        OIVCommands::GetKnownFileTypes(fileTypesAnsi);
        fKnownFileTypes = LLUtils::StringUtility::ToWString(LLUtils::StringUtility::ToLower(fileTypesAnsi));
        
        auto fileTypeList =  LLUtils::StringUtility::split(fKnownFileTypes, L';');
        for (const auto& fileType : fileTypeList)
            fKnownFileTypesSet.insert(fileType);
    	
        fNotificationIconID =  fNotificationIcons.AddIcon(MAKEINTRESOURCE(IDI_APP_ICON), LLUTILS_TEXT("Open Image Viewer"));
        fNotificationIcons.OnNotificationIconEvent.Add(std::bind(&TestApp::OnNotificationIcon, this, std::placeholders::_1));

        fNotificationContextMenu = std::make_unique < ContextMenu<int>> (fWindow.GetHandle());
        fNotificationContextMenu->AddItem(OIV_TEXT("Quit"), int{});

        using namespace LInput;
        fRawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls, RawInput::GenericDesktopControlsUsagePage::Mouse, RawInput::Flags::EnableBackground);

        fRawInput.OnInput.Add(std::bind(&TestApp::OnRawInput, this,std::placeholders::_1));
        fRawInput.Enable(true);

        fMouseClickEventHandler.OnMouseClickEvent.Add(std::bind(&TestApp::OnMouseMultiClick, this, std::placeholders::_1));

        LoadSettings(true);

        if (fAllowDynamicSettings)
            fCOnfigurationFolderID = fFileWatcher.AddFolder(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/."));
    }

    template <typename value_type, typename container_type,typename target_type>
    bool LoadValue(const container_type& container, std::string name, target_type& value)
    {
        auto it = container.find(name);
        if (it != container.end())
        {
            value = static_cast<target_type>(std::get<value_type>(it->second));
            return true;
        }
        else
        {
            return false;
        }

    }

    void TestApp::LoadSettings(bool startup)
    {
        auto settings = ConfigurationLoader::LoadSettings();

        LoadValue<ConfigurationLoader::Float>(settings, "/viewsettings/maxzoom", fMaxPixelSize);
        LoadValue<ConfigurationLoader::Float>(settings, "/viewsettings/imagemargins/x", fImageMargins.x);
        LoadValue<ConfigurationLoader::Float>(settings, "/viewsettings/imagemargins/y", fImageMargins.y);
        LoadValue<ConfigurationLoader::Integral>(settings, "/viewsettings/minimagesize", fMinImageSize);
        LoadValue<ConfigurationLoader::Integral>(settings, "/viewsettings/slideshowinterval", fSlideShowIntervalms);
        LoadValue<ConfigurationLoader::Integral>(settings, "/viewsettings/quickbrowsedelay", fQuickBrowseDelay);
        LoadValue<ConfigurationLoader::Bool>(settings, "/viewsettings/autoloadchangedfile", fAutoLoadChangedFile);

        //Auto scroll metrics

        AutoScroll::ScrollMetrics metrics{};

        LoadValue<ConfigurationLoader::Integral>(settings, "/autoscroll/deadzoneradius", metrics.deadZoneRadius);
        LoadValue<ConfigurationLoader::Float>   (settings, "/autoscroll/speedfactorin", metrics.speedInFactorIn);
        LoadValue<ConfigurationLoader::Float>   (settings, "/autoscroll/speedfactorout", metrics.speedFactorOut);
        LoadValue<ConfigurationLoader::Integral>(settings, "/autoscroll/speedfactorrange", metrics.speedFactorRange);
        LoadValue<ConfigurationLoader::Integral>(settings, "/autoscroll/maxspeed", metrics.maxSpeed);

        fAutoScroll->SetScrollMetrics(metrics);

        if (startup)
        {
            LoadValue<ConfigurationLoader::Bool>(settings, "/system/allowdynamicsettings", fAllowDynamicSettings);
        }

#if _DEBUG
        fAllowDynamicSettings = true;
#endif
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

    void TestApp::Destroy()
    {
        // destroy OIV resources before destroying OIV.
        fImageState.ClearAll();
        fLabelManager.RemoveAll();
        // Destroy OIV when window is closed.
        OIVCommands::ExecuteCommand(OIV_CMD_Destroy, &NullCommand, &NullCommand);
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
        return fImageState.GetVisibleImage()->GetImageProperties().filterType;
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
        
        while ((isLoaded = LoadFile(*it, true)) == false);


        if (isLoaded)
        {
            assert(fileIndex >= 0 && fileIndex < static_cast<FileIndexType>(totalFiles));
            fCurrentFileIndex = fileIndex;
            UpdateTitle();
        }
        return isLoaded;
    }
    
    void TestApp::ToggleFullScreen(bool multiFullScreen)
    {
        fWindow.ToggleFullScreen(multiFullScreen);
		Center();
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
        fImageState.GetVisibleImage()->GetImageProperties().filterType =  std::clamp(filterType
            , FT_None, static_cast<OIV_Filter_type>( FT_Count - 1));

        fImageState.GetVisibleImage()->Update();
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
        BindingElement bindings;
        return  fKeyBindings.GetBinding(keyCombination, bindings) && ExecutePredefinedCommand(bindings.commandDescription);
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
            SetOffset(panAmount + fImageState.GetOffset()) ;
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
            PointF64 ratio = PointF64(clientSize.cx, clientSize.cy) / GetImageSize(ImageSizeType::Transformed);
            double zoom = std::min(ratio.x, ratio.y);
            fRefreshOperation.Begin();
            fIsLockFitToScreen = true;
            SetZoomInternal(zoom, -1, -1, true);
            Center();
            fRefreshOperation.End();
        }
    }
    
    LLUtils::PointF64 TestApp::GetImageSize(ImageSizeType imageSizeType)
    {
        using namespace LLUtils;
        switch (imageSizeType)
        {
        case ImageSizeType::Original:
            return  fImageState.GetImage(ImageChainStage::SourceImage) != nullptr ? PointF64(fImageState.GetImage(ImageChainStage::SourceImage)->GetDescriptor().Width, fImageState.GetImage(ImageChainStage::SourceImage)->GetDescriptor().Height) : PointF64(0, 0);
        case ImageSizeType::Transformed:
            return PointF64(fImageState.GetImage(ImageChainStage::Deformed)->GetDescriptor().Width, fImageState.GetImage(ImageChainStage::Deformed)->GetDescriptor().Height);
        case ImageSizeType::Visible:
            return fImageState.GetVisibleSize();

        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    void TestApp::SaveImageSpaceSelection()
    {
            if (fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
             fImageSpaceSelection =  static_cast<LLUtils::RectI32>(ClientToImage(fSelectionRect.GetSelectionRect()).Round());
    }

    void TestApp::LoadImageSpaceSelection()
    { 
        if (fImageSpaceSelection.IsEmpty() == false)
        {
            LLUtils::RectI32 r = static_cast<LLUtils::RectI32>(ImageToClient(static_cast<LLUtils::RectF64>(fImageSpaceSelection)));
            fSelectionRect.UpdateSelection(r);
        }
    }

    void TestApp::CancelSelection()
    {
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
                    OIV_CMD_TexelInfo_Request texelInfoRequest = { fImageState.GetImage(ImageChainStage::Deformed)->GetDescriptor().ImageHandle
               ,static_cast<uint32_t>(storageImageSpace.x)
               ,static_cast<uint32_t>(storageImageSpace.y) };
                    OIV_CMD_TexelInfo_Response  texelInfoResponse;

                    if (OIVCommands::ExecuteCommand(OIV_CMD_TexelInfo, &texelInfoRequest, &texelInfoResponse) == RC_Success)
                    {
                        std::wstring message = StringUtility::ConvertString<OIVString>(OIVHelper::ParseTexelValue(texelInfoResponse));
                        OIVString txt = LLUtils::StringUtility::ConvertString<OIVString>(message);
                        fVirtualStatusBar.SetText("texelValue", txt);
                        fVirtualStatusBar.SetOpacity("texelValue", 1.0);
                        fRefreshOperation.Queue();
                    }

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

        CmdSetClientSizeRequest req{ static_cast<uint16_t>(size.cx),
            static_cast<uint16_t>(size.cy) };

        OIVCommands::ExecuteCommand(CMD_SetClientSize,
            &req, &NullCommand);
        //UpdateCanvasSize();
		AutoPlaceImage();
        auto point = static_cast<LLUtils::PointI32>(fWindow.GetCanvasSize());
        fVirtualStatusBar.ClientSizeChanged(point);
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

    
    void TestApp::TransformImage(OIV_AxisAlignedRotation relativeRotation, OIV_AxisAlignedFlip flip)
    {
	   fRefreshOperation.Begin();
       SetResamplingEnabled(false);
       fImageState.Transform(relativeRotation, flip);
	   AutoPlaceImage(true);
	   RefreshImage();
	   fRefreshOperation.End();
       SetResamplingEnabled(true);
    }

    void TestApp::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat)
    {
        std::shared_ptr<OIVRawImage>  rawImage = std::make_shared<OIVRawImage>(ImageSource::Clipboard);
        RawBufferParams params;
        params.width = width;
        params.height = height;
        params.rowPitch = rowPitch;
        params.texelFormat = texelFormat;
        params.buffer = buffer;

        ResultCode result = rawImage->Load(params);
        fImageState.SetOpenedImage(rawImage);

        FinalizeImageLoadThreadSafe(result);
    }

    bool TestApp::PasteFromClipBoard()
    {
        bool success = false;;
        if (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5))
        {
            if (OpenClipboard(NULL))
            {
                HANDLE hClipboard = GetClipboardData(CF_DIB);
                
                //LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "Unsupported clipboard bitmap format type");

                if (!hClipboard)
                {
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "Unsupported clipboard bitmap format type");
                    //hClipboard = GetClipboardData(CF_DIBV5);
                }

                if (hClipboard != NULL && hClipboard != INVALID_HANDLE_VALUE)
                {
                    void* dib = GlobalLock(hClipboard);

                    if (dib)
                    {
                        const tagBITMAPINFO * bitmapInfo = reinterpret_cast<tagBITMAPINFO*>(dib);
                        const BITMAPINFOHEADER* info = &(bitmapInfo->bmiHeader);
                        uint32_t rowPitch = LLUtils::Utility::Align<uint32_t>(info->biWidth * (info->biBitCount / 8), 4);

                        const std::byte* bitmapBitsconst = reinterpret_cast<const std::byte*>(info + 1);
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


                        LoadRaw(bitmapBits
                            , info->biWidth
                            , info->biHeight
                            , rowPitch
                            , info->biBitCount == 24 ? OIV_TexelFormat::TF_I_B8_G8_R8 : OIV_TexelFormat::TF_I_B8_G8_R8_A8);

                        GlobalUnlock(dib);
                        success = true;
                    }
                }

                CloseClipboard();
            }
        }

        return success;
    }


    void TestApp::CopyVisibleToClipBoard()
    {
        LLUtils::RectI32 imageRectInt = static_cast<LLUtils::RectI32>(ClientToImage(fSelectionRect.GetSelectionRect()));
        ImageHandle croppedHandle = ImageHandleNull;
        ImageHandle clipboardCompatibleHandle = ImageHandleNull;
        ResultCode result = OIVCommands::CropImage(fImageState.GetImage(ImageChainStage::Rasterized)->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);

        if (result == RC_Success)
        {
            OIVHandleImageSharedPtr croppedImage = std::make_shared<OIVHandleImage>(croppedHandle);
            //2. Flip the image vertically and convert it to BGRA for the clipboard.
            result = OIVCommands::TransformImage(croppedHandle, OIV_AxisAlignedRotation::AAT_None, OIV_AxisAlignedFlip::AAF_Vertical, clipboardCompatibleHandle);
            if (result == RC_Success)
            {
                OIVHandleImageSharedPtr compatibleImage = std::make_shared<OIVHandleImage>(clipboardCompatibleHandle);
                result = OIVCommands::ConvertImage(clipboardCompatibleHandle, OIV_TexelFormat::TF_I_B8_G8_R8_A8, false, clipboardCompatibleHandle);
                compatibleImage = std::make_shared<OIVHandleImage>(clipboardCompatibleHandle);
                //3. Get image pixel buffer and Copy to clipboard.

                OIV_CMD_GetPixels_Request requestGetPixels;
                OIV_CMD_GetPixels_Response responseGetPixels;

                requestGetPixels.handle = clipboardCompatibleHandle;

                if (OIVCommands::ExecuteCommand(OIV_CMD_GetPixels, &requestGetPixels, &responseGetPixels) == RC_Success)
                {
                    struct hDibDelete
                    {
                        bool dlt;
                        HANDLE mHande;
                        ~hDibDelete()
                        {
                            if (dlt && mHande)
                            {
                                GlobalFree(mHande);
                            }
                        }
                    };


                    uint32_t width = responseGetPixels.width;
                    uint32_t height = responseGetPixels.height;
                    uint8_t bpp;
                    OIV_Util_GetBPPFromTexelFormat(responseGetPixels.texelFormat, &bpp);

                    HANDLE hDib = LLUtils::PlatformUtility::CreateDIB(width, height, bpp, responseGetPixels.pixelBuffer);

                    hDibDelete deletor = { true,hDib };


                    if (::OpenClipboard(nullptr))
                    {
                        if (SetClipboardData(CF_DIB, hDib) != nullptr)
                        {
                            //succeeded do not free hDib.
                            deletor.dlt = false;
                        }
                        else
                        {
                            LL_EXCEPTION_SYSTEM_ERROR("Unable to set clipboard data.");
                        }
                        if (CloseClipboard() == FALSE)
                            LL_EXCEPTION_SYSTEM_ERROR("can not close clipboard.");

                    }
                    else
                    {
                        LL_EXCEPTION_SYSTEM_ERROR("Can not open clipboard.");
                    }
                }
            }
        }
    }

    void TestApp::CropVisibleImage()
    {
        LLUtils::RectI32 imageRectInt = static_cast<LLUtils::RectI32>(ClientToImage(fSelectionRect.GetSelectionRect()));
        
        ImageHandle croppedHandle;

        
        ResultCode result = OIVCommands::CropImage(fImageState.GetImage(ImageChainStage::Deformed)->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);
        
        if (result == RC_Success)
        {
            std::shared_ptr<OIVHandleImage> handleImage = std::make_shared<OIVHandleImage>(croppedHandle);
            fImageState.SetImageChainRoot(handleImage);
            CancelSelection();
        }

        FinalizeImageLoadThreadSafe(result);

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
            UpdateWindowSize();
            fRefreshOperation.Queue();
            break;
        }
        return retValue;
    }

    void TestApp::SetTopMostUserMesage()
    {
        std::wstring message = L"Top most ending in..." + std::to_wstring(fTopMostCounter);
        SetUserMessage(message, -1);
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
                HideUserMessageGradually();
            }
            else
                SetTopMostUserMesage();
        }

    }

    void TestApp::ProcessCurrentFileChanged()
    {
        using namespace std::string_literals;

        if (fAutoLoadChangedFile == true)
        {
            LoadFile(GetOpenedFileName(), false);
        }
        else
        {
            if (fFileReloadPending == false)
            {
                fFileReloadPending = true;
                int mbResult = MessageBox(fWindow.GetHandle(), (L"Reload the file: "s + GetOpenedFileName()).c_str(), L"File is changed outside of OIV", MB_YESNO);
                fFileReloadPending = false;
                if (mbResult == IDYES)
                {
                    LoadFile(GetOpenedFileName(), false);
                }
                else if (mbResult == IDNO)
                {

                }
            }
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
        
        case Win32::UserMessage::PRIVATE_WN_NOTIFY_LOADED:
            FinalizeImageLoadThreadSafe(static_cast<ResultCode>( evnt->message.wParam));
        break;
        
        case Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL:
            fAutoScroll->PerformAutoScroll();
            break;
        case Win32::UserMessage::PRIVATE_WN_NOTIFY_USER_MESSAGE:
            SetUserMessage(fLastMessageForMainThread, static_cast<int32_t>(uMsg.wParam));
            break;
        case Win32::UserMessage::PRIVATE_WM_NOTIFY_FILE_CHANGED:
            OnFileChangedImpl(reinterpret_cast<FileWatcher::FileChangedEventArgs*>(uMsg.wParam));
            break;

            case WM_COPYDATA:
            {
                COPYDATASTRUCT* cds = (COPYDATASTRUCT*)uMsg.lParam;
                if (uMsg.wParam == OIV::Win32::UserMessage::PRIVATE_WM_LOAD_FILE_EXTERNALLY)
                {
                    wchar_t* fileToLoad = reinterpret_cast<wchar_t*>(cds->lpData);
                    LoadFile(fileToLoad, false);
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


    bool TestApp::HandleFileDragDropEvent(const ::Win32::EventDdragDropFile* event_ddrag_drop_file)
    {
        if (LoadFile(event_ddrag_drop_file->fileName, false))
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

    void TestApp::SetUserMessageThreadSafe(const std::wstring& message, int32_t hideDelay)
    {
        if (IsMainThread() == true)
		{
            SetUserMessage(message, hideDelay);
		}
        else
        {
            fLastMessageForMainThread = message;
            // Wait for the main window to get initialized.
            std::unique_lock<std::mutex> ul(fMutexWindowCreation);
			
            PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_NOTIFY_USER_MESSAGE, hideDelay, 0);
        }
    }

    void TestApp::SetUserMessage(const std::wstring& message,int32_t hideDelay )
    {
        OIVTextImage* userMessage = fLabelManager.GetTextLabel("userMessage");
        if (userMessage == nullptr)
        {
            userMessage = fLabelManager.GetOrCreateTextLabel("userMessage");
            CreateTextParams& textOptions = userMessage->GetTextOptions();
            textOptions.backgroundColor = 0;
            textOptions.fontPath = LabelManager::sFontPath;
            textOptions.fontSize = 12;
            textOptions.outlineWidth = 2;
            textOptions.renderMode = OIV_PROP_CreateText_Mode::CTM_AntiAliased;

            OIV_CMD_ImageProperties_Request& properties = userMessage->GetImageProperties();
            properties.position = { 20,20 };
            properties.filterType = OIV_Filter_type::FT_None;
            properties.imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
            properties.scale = 1.0;
            properties.opacity = 1.0;
            properties.visible = true;
        }


        userMessage->GetImageProperties().opacity = 1.0;

        CreateTextParams& textOptions = userMessage->GetTextOptions();
        
        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += message;
        textOptions.text = LLUtils::StringUtility::ConvertString<OIVString>(wmsg);

        if (userMessage->Update() == RC_Success)
        {
            fRefreshOperation.Queue();

            int32_t messageDelay = 0;
            if (hideDelay == 0)     //Auto hide delay
            {
                messageDelay = std::max(fMinDelayRemoveMessage, static_cast<uint32_t>(message.length() * fDelayPerCharacter));
            }
            else
            {
                hideDelay = messageDelay;
            }

            if (messageDelay > 0 )
                fTimerHideUserMessage.SetInterval(messageDelay);
        }
    }

    
    
    void TestApp::SetDebugMessage(const std::string& message)
    {
        OIVTextImage* debugMessage =  fLabelManager.GetOrCreateTextLabel("debugLabel");
        

        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += LLUtils::StringUtility::ToWString(message);

        OIVString txt = LLUtils::StringUtility::ConvertString<OIVString>(wmsg);
        CreateTextParams& textOptions = debugMessage->GetTextOptions();
        textOptions.text = txt;
        textOptions.backgroundColor = LLUtils::Color(0, 0, 0, 180).colorValue;
        textOptions.fontPath = LabelManager::sFontPath;

        if (debugMessage->Update() == RC_Success)
        {
            fRefreshOperation.Queue();
        }
    }
    
    void TestApp::HideUserMessageGradually()
    {
        OIVTextImage* userMessage = fLabelManager.GetTextLabel("userMessage");
        OIV_CMD_ImageProperties_Request& imageProperties = userMessage->GetImageProperties();

        if (imageProperties.opacity == 1.0)
        {
            // start exponential fadeout after 'fDelayRemoveMessage' milliseconds.
            fTimerHideUserMessage.SetInterval(5);
            imageProperties.opacity = 0.99;
        }

        imageProperties.opacity = imageProperties.opacity * 0.8;
        if (imageProperties.opacity < 0.01)
        {
            imageProperties.opacity = 0;
            fTimerHideUserMessage.SetInterval(0);
        }

        if (userMessage->Update() == RC_Success)
            fRefreshOperation.Queue();
        
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

    void TestApp::ShowImageInfo()
    {
        if (IsImageOpen())
        {
            std::wstring imageInfoString = MessageHelper::CreateImageInfoMessage(fImageState.GetImage(ImageChainStage::SourceImage));
            OIVTextImage* imageInfoText = fLabelManager.GetOrCreateTextLabel("imageInfo");
            CreateTextParams& params = imageInfoText->GetTextOptions();
            
            params.text = imageInfoString;
            params.backgroundColor = LLUtils::Color(0,0,0,127).colorValue;
            params.fontPath = LabelManager::sFixedFontPath;
            params.fontSize = 12;
            params.renderMode = OIV_PROP_CreateText_Mode::CTM_AntiAliased;
            params.outlineWidth = 2;

            imageInfoText->GetImageProperties().position = { 20,60 };

            if (imageInfoText->Update() == RC_Success)
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
        OIVString txt = LLUtils::StringUtility::ConvertString<OIVString>(wmsg);
        
        CreateTextParams& params = welcomeMessage->GetTextOptions();
        
        params.text = txt;
        params.backgroundColor = LLUtils::Color(0).colorValue;
        params.fontPath = LabelManager::sFontPath;
        params.fontSize = 44;
        params.renderMode = OIV_PROP_CreateText_Mode::CTM_AntiAliased;
        params.outlineWidth = 3;


        if (welcomeMessage->Update() == RC_Success) // create text
        {
            
            //get the text size to reposition on screen
            using namespace LLUtils;
            PointI32 clientSize = fWindow.GetCanvasSize();
            PointI32 center = (clientSize - PointI32(welcomeMessage->GetDescriptor().Width, welcomeMessage->GetDescriptor().Height)) / 2;
            welcomeMessage->GetImageProperties().position = { static_cast<PointF64>(center) };
            if (welcomeMessage->Update() == RC_Success) // resposition text
                fRefreshOperation.Queue();
        }
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

