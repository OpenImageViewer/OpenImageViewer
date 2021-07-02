#include <limits>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "TestApp.h"
#include <LLUtils/StringUtility.h>
#include "win32/Win32Window.h"
#include <windows.h>
#include "win32/MonitorInfo.h"
#include <LLUtils/FileSystemHelper.h>

#include <functions.h>
#include <LLUtils/Exception.h>
#include "win32/Win32Helper.h"
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include "win32/UserMessages.h"
#include "UserSettings.h"
#include "OIVCommands.h"
#include <LLUtils/Rect.h>
#include "Helpers/OIVHelper.h"
#include "Keyboard/KeyCombination.h"
#include "Keyboard/KeyBindings.h"
#include "Keyboard/KeyDoubleTap.h"
#include "SelectionRect.h"
#include "Helpers\PhotoshopFinder.h"
#include <Version.h>
#include "OIVImage\OIVHandleImage.h"
#include "OIVImage\OIVFileImage.h"
#include "OIVImage\OIVRawImage.h"
#include "Helpers\OIVImageHelper.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/Logging/LogFile.h>
#include "ContextMenu.h"

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
			CloseApplication();
        }
        else if (type == "grid")
        {
            ToggleGrid();
            result.resValue = L"Grid ";
            result.resValue += fIsGridEnabled == true ? L"on" : L"off";
        }
        else if (type == "slideShow")
        {
            ToggleSlideShow();
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
            case FullSceenState::MultiScreen:
                result.resValue = L"Multi full screen";
                break;
            case FullSceenState::SingleScreen:
                result.resValue = L"Full screen";
                break;
            case FullSceenState::Windowed:
                result.resValue = L"Windowed";
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
            }
        }
    }

    void TestApp::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        OIVTextImage* text = fLabelManager.GetTextLabel("keyBindings");

        if (text != nullptr)
        {
            fLabelManager.Remove("keyBindings");
            fRefreshOperation.Queue();
            return;
        }

        text = fLabelManager.GetOrCreateTextLabel("keyBindings");

        using namespace std;
        
        struct ColumnInfo
        {
             size_t maxFirstLength;
             size_t maxSecondLength;
        };
        //TODO: get max lines from window height
        const int MaxLines = 24;
        const int totalColumns = static_cast<int>(std::ceil(static_cast<double>(fCommandDescription.size()) / MaxLines));
        std::vector<ColumnInfo> columnInfo(totalColumns);
        int currentcolumn = 0;
        int currentLine = 0;
        
        for (const CommandDesc& desc : fCommandDescription)
        {
            
            columnInfo[currentcolumn].maxFirstLength = std::max(columnInfo[currentcolumn].maxFirstLength, desc.description.length());
            columnInfo[currentcolumn].maxSecondLength = std::max(columnInfo[currentcolumn].maxSecondLength, desc.keybindings.length());
            currentLine++;
            if (currentLine >= MaxLines)
            {
                currentLine = 0;
                currentcolumn++;
            }
        }
        
        currentcolumn = 0;
        currentLine = 0;

        vector<std::string> lines(MaxLines);


        for (const CommandDesc& desc : fCommandDescription)
        {
            stringstream ss;
            ss << "<textcolor=#ff8930>" << desc.description;
            

            size_t currentLength = desc.description.length();
            while (currentLength++ < columnInfo[currentcolumn].maxFirstLength)
                ss << ".";

            ss << "..." << "<textcolor=#98f733>" << desc.keybindings ;
            if (currentcolumn < totalColumns - 1)
            {
                size_t currentLength = desc.keybindings.length();
                while (currentLength++ < columnInfo[currentcolumn].maxSecondLength)
                    ss << " ";
                
                ss << "      ";
            }

            lines[currentLine] += ss.str();

            currentLine++;
            if (currentLine >= MaxLines)
            {
                currentLine = 0;
                currentcolumn++;
            }
        }

        stringstream ss1;
        for (const std::string& line : lines)
            ss1 << line << "\n";

        std::string message = ss1.str();
        if (message[message.size() - 1] == '\n')
            message.erase(message.size() - 1);


        

        CreateTextParams& requestText = text->GetTextOptions();

        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += LLUtils::StringUtility::ToWString(message);
        
        requestText.text = LLUtils::StringUtility::ConvertString<OIVString>(wmsg);
        requestText.backgroundColor = LLUtils::Color(0, 0, 0, 216).colorValue;
        requestText.fontPath = LabelManager::sFixedFontPath;
        requestText.fontSize = 12;
        requestText.renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;
        requestText.outlineWidth = 2;


        OIV_CMD_ImageProperties_Request& imageProperties = text->GetImageProperties();
        imageProperties.position = { 20,60 };
        imageProperties.filterType = OIV_Filter_type::FT_None;
        //imageProperties.imageHandle = responseText.imageHandle;
        imageProperties.imageRenderMode = IRM_Overlay;
        imageProperties.scale = 1.0;
        imageProperties.opacity = 1.0;
        imageProperties.visible = true;

        if ( text->Update() == RC_Success)
            fRefreshOperation.Queue();

    }

    void TestApp::CMD_OpenFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& response)
    {
        std::wstring fileName = Win32Helper::OpenFile(fWindow.GetHandle());
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


    void TestApp::CMD_ToggleColorCorrection(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
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

        result.resValue = LLUtils::StringUtility::ToWString(request.description);
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

        ss << "<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(request.description) << "<textcolor=#7672ff>" << " (" << amountVal << " pixels)";

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
                result.resValue = LLUtils::StringUtility::ToWString(request.description);
            }
        }
        else if (cmd == "selectedArea")
        {
            CopyVisibleToClipBoard();
            result.resValue = LLUtils::StringUtility::ToWString(request.description);
        }
    }


    void TestApp::CMD_PasteFromClipboard(const CommandManager::CommandRequest& request,
        CommandManager::CommandResult& result)
    {
        if (PasteFromClipBoard())
            result.resValue = LLUtils::StringUtility::ToWString(request.description);
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
            result.resValue = LLUtils::StringUtility::ToWString(request.description);
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

        ss << "<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(request.description);// << "<textcolor=#7672ff>" << " (" << amountVal << " pixels)";

        result.resValue = ss.str();

    }

    void TestApp::CMD_Navigate(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace std;
        string amount = request.args.GetArgValue("amount");
        if (amount == "start")
            JumpFiles(FileIndexStart);

        else if (amount == "end")
            JumpFiles(FileIndexEnd);
        else
        {
            int amountVal = std::stoi(amount, nullptr);
            JumpFiles(amountVal);
        }

    }

    void TestApp::CMD_Shell(const CommandManager::CommandRequest& request,
        CommandManager::CommandResult& result)
    {
        using namespace std;
        string cmd = request.args.GetArgValue("cmd");
        result.resValue = LLUtils::StringUtility::ToWString(request.description);

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
                Win32Helper::BrowseToFile(GetOpenedFileName());
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

	std::wstring TestApp::GetLogFilePath()
	{
		return LLUtils::PlatformUtility::GetAppDataFolder() + L"/OIV/oiv.log";
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
        :fRefreshOperation(std::bind(&TestApp::OnRefresh, this))
        , fPreserveImageSpaceSelection(std::bind(&TestApp::OnPreserveSelectionRect, this))
        , fRefreshTimer(std::bind(&TestApp::OnRefreshTimer, this))
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


        std::vector<CommandDesc> localDesc

        {
            //general
             { "Key bindings","cmd_toggle_keybindings","" ,"F1" }
            ,{ "Open file","cmd_open_file","" ,"Control+O" }
            ,{ "Delete file","cmd_delete_file","type=recyclebin" ,"GreyDelete" }
            ,{ "Delete file permanently","cmd_delete_file","type=permanently" ,"Shift+GreyDelete" }

            //View state
            ,{ "Toggle full screen","cmd_view_state","type=toggleFullScreen" ,"Alt+Enter" }
            ,{ "Toggle status bar","cmd_view_state","type=toggleStatusBar" ,"Tab" }
            ,{ "Toggle multi full screen","cmd_view_state","type=toggleMultiFullScreen" ,"Alt+Shift+Enter" }
            ,{ "Borders","cmd_view_state","type=toggleBorders" ,"B" }
            ,{ "Quit","cmd_view_state","type=quit" ,"Escape" }
            ,{ "Grid","cmd_view_state","type=grid" ,"G" }
            ,{ "Slide show","cmd_view_state","type=slideShow" ,"Space" }
            ,{ "Toggle normalization","cmd_view_state","type=toggleNormalization" ,"N" }
            ,{ "Image filter up","cmd_view_state","type=imageFilterUp" ,"Period" }
            ,{ "Image filter down","cmd_view_state","type=imageFilterDown" ,"Comma" }
            ,{ "Toggle reset offset on load","cmd_view_state","type=toggleresetoffset" ,"Backslash" }
            ,{ "Toggle transparency mode","cmd_view_state","type=toggletransparencymode" ,"T" }
            ,{ "Toggle downsampling technique","cmd_view_state","type=toggledownsamplingtechnique" ,"M" }

            //Color correction
            ,{ "Increase Gamma","cmd_color_correction","type=gamma;op=add;val=0.05" ,"Q" }
            ,{ "Decrease Gamma","cmd_color_correction","type=gamma;op=subtract;val=0.05" ,"A" }
            ,{ "Increase Exposure","cmd_color_correction","type=exposure;op=add;val=0.05" ,"W" }
            ,{ "Decrease Exposure","cmd_color_correction","type=exposure;op=subtract;val=0.05" ,"S" }
            ,{ "Increase Offset","cmd_color_correction","type=offset;op=add;val=0.01" ,"E" }
            ,{ "Decrease Offset","cmd_color_correction","type=offset;op=subtract;val=0.01" ,"D" }
            ,{ "Increase Saturation","cmd_color_correction","type=saturation;op=add;val=0.05" ,"R" }
            ,{ "Decrease Saturation","cmd_color_correction","type=saturation;op=subtract;val=0.05" ,"F" }
            ,{ "Toggle color correction","cmd_toggle_correction","" ,"Z" }

            //Axis aligned transformations
            ,{ "Horizontal flip","cmd_axis_aligned_transform","type=hflip" ,"H" }
            ,{ "Vertical flip","cmd_axis_aligned_transform","type=vflip" ,"V" }
            ,{ "Rotate clockwise","cmd_axis_aligned_transform","type=rotatecw" ,"RBracket" }
            ,{ "Rotate counter clockwise","cmd_axis_aligned_transform","type=rotateccw" ,"LBracket" }

            //Pan
            ,{ "Pan up","cmd_pan","direction=up;amount=1" ,"Numpad8" }
            ,{ "Pan down","cmd_pan","direction=down;amount=1" ,"Numpad2" }
            ,{ "Pan left","cmd_pan","direction=left;amount=1" ,"Numpad4" }
            ,{ "Pan right","cmd_pan","direction=right;amount=1" ,"Numpad6" }

            //Zoom
            ,{ "Zoom in ","cmd_zoom","val=0.1" ,"Add" }
            ,{ "Zoom out","cmd_zoom","val=-0.1" ,"Subtract" }

            //Image placement
            ,{ "Original size","cmd_placement","cmd=originalSize" ,"Multiply" }
            ,{ "Fit to screen","cmd_placement","cmd=fitToScreen" ,"KeyPadDivide" }
            ,{ "Center","cmd_placement","cmd=center" ,"Numpad5" }

            //Image manipulation
            ,{ "Crop to selected area","cmd_imageManipulation","cmd=cropSelectedArea" ,"C" }
            ,{ "Select all","cmd_imageManipulation","cmd=selectAll" ,"Control+A" }

            //Shell
            ,{ "Open new window","cmd_shell","cmd=newWindow" ,"Control+N" }
            ,{ "Open in photoshop","cmd_shell","cmd=openPhotoshop" ,"P" }

            //Clipboard
            ,{ "Copy selected area to clipboard","cmd_copyToClipboard","cmd=selectedArea" ,"Control+C" }
            ,{ "Copy file name to clipboard","cmd_copyToClipboard","cmd=fileName" ,"Control+Shift+C" }
            ,{ "Paste image from clipboard","cmd_pasteFromClipboard","" ,"Control+V" }
            ,{ "Previous","cmd_navigate","amount=-1" ,"GREYLEFT" }
            ,{ "Previous","cmd_navigate","amount=-1" ,"GREYPGUP" }
            ,{ "Next","cmd_navigate","amount=1" ,"GREYRIGHT" }
            ,{ "Next","cmd_navigate","amount=1" ,"GREYPGDN" }


            ,{ "First","cmd_navigate","amount=start" ,"GREYHOME" }
            ,{ "Last","cmd_navigate","amount=end" ,"GREYEND" }
        };

        fCommandDescription = localDesc;

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


        for (const CommandDesc& desc : fCommandDescription)
            fKeyBindings.AddBinding(KeyCombination::FromString(desc.keybindings)
                , { desc.description, desc.command,desc.arguments });
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
            
            fRefreshTimer.Enable(false);

            if (microsecSinceLastRefresh > windowTimeInMicroSeconds)
            {
                //Refresh immediately
                OIVCommands::Refresh();
                fLastRefreshTime = now;
                //Clear last image chain if exists, this operation is deffered to this moment to display the new image faster.
                fImageState.ResetPreviousImageChain();
            }
            else
            {
                //Don't refresh now, restrat refresh timer
                fRefreshTimer.SetDelay(static_cast<DWORD>((windowTimeInMicroSeconds - microsecSinceLastRefresh) / 1000));
                fRefreshTimer.Enable(true);
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
        //Perform refresh in the main thread.
        PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WM_REFRESH_TIMER, 0, 0);
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
        const static std::wstring cachedVersionString = L"OpenImageViewer " + std::to_wstring(OIV_VERSION_MAJOR) + L'.' + std::to_wstring(OIV_VERSION_MINOR)
        
 // If not official release add revision and build number
#if OIV_OFFICIAL_RELEASE == 0
        + L"." +  LLUtils::StringUtility::ToWString(OIV_VERSION_REVISION) + L"." + std::to_wstring(OIV_VERSION_BUILD)
#endif			

 // If not official build, i.e. from unofficial / unknown source, add an "UNOFFICIAL" remark.
#if OIV_OFFICIAL_BUILD == 0
            + L" - UNOFFICIAL"
#endif
			;

		std::wstring title;
		if (GetOpenedFileName().empty() == false)
			title = GetOpenedFileName() + L" - ";
		title += cachedVersionString;


		fWindow.SetTitle(title);
    }


    void TestApp::UpdateStatusBar()
    {
        fWindow.SetStatusBarText(fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescription(), 0, 0);
    }
   
    void TestApp::UnloadOpenedImaged()
    {
        fImageState.GetWorkingImageChain().Reset();
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


        if (shResult == 0 && file_op.fAnyOperationsAborted == FALSE)
        {
            bool isLoaded = JumpFiles(1);
            if (isLoaded == false)
                isLoaded = JumpFiles(-1);

            ReloadFileInFolder();

            if (isLoaded == false)
                UnloadOpenedImaged();
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
        {
            using namespace std::filesystem;
            std::wstringstream ss;
            ss << L"File: ";
            path p = GetOpenedFileName();
            ss << p.parent_path().wstring() << path::preferred_separator << L"<textcolor=#ff00ff>" << p.stem().wstring() << L"<textcolor=#00ff00>" << p.extension().wstring();
            SetUserMessage(ss.str());
        }
    }

    void TestApp::FinalizeImageLoad(ResultCode result)
    {
        if (result == RC_Success)
        {
            // Enter this function only from the main thread.
            assert("TestApp::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());
            
            fRefreshOperation.Begin();
			
            fImageState.ResetUserState();
            RefreshImage(); // actual refresh is deferred due to 'fRefreshOperation.Begin'.

			
            if (fResetTransformationMode == ResetTransformationMode::ResetAll)
                FitToClientAreaAndCenter();
            
            AutoPlaceImage();

            if (fIsInitialLoad == false)
                UpdateUIFileIndex();

            fWindow.SetShowImageControl(fImageState.GetOpenedImage()->GetDescriptor().NumSubImages > 0);
            UnloadWelcomeMessage();
            DisplayOpenedFileName();

            if (fIsInitialLoad == false)
            {
                fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), true);
                fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), true);
            }
        	
            //Don't refresh on initial file, wait for WM_SIZE
            fRefreshOperation.End(fIsInitialLoad == false);
        }

        if (fIsInitialLoad == true)
        {
            fWindow.SetVisible(true);
            fIsInitialLoad = false;
        }
    }

    void TestApp::AddImageToControl(OIVBaseImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        OIV_CMD_GetPixels_Request pixelsRequest;
        OIV_CMD_GetPixels_Response pixelsResponse;
        
        OIVBaseImageSharedPtr bgraImage = OIVImageHelper::ConvertImage(image, TF_I_B8_G8_R8_A8, false);

        OIVBaseImageSharedPtr systemCompatibleImage;
        
        
        ImageHandle windowCompatibleBitmapHandle;
        if (OIVCommands::TransformImage(bgraImage->GetDescriptor().ImageHandle, OIV_AxisAlignedRotation::AAT_None, OIV_AxisAlignedFlip::AAF_Vertical, windowCompatibleBitmapHandle) == RC_Success)
        {
            systemCompatibleImage = std::make_shared<OIVHandleImage>(windowCompatibleBitmapHandle);
        }


        pixelsRequest.handle = systemCompatibleImage->GetDescriptor().ImageHandle;
            ResultCode result = OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_GetPixels, &pixelsRequest, &pixelsResponse);
        uint8_t bpp;
        OIV_Util_GetBPPFromTexelFormat(pixelsResponse.texelFormat, &bpp);


        BitmapBuffer bitmapBuffer = {};
        bitmapBuffer.bitsPerPixel = bpp;
        bitmapBuffer.buffer = pixelsResponse.pixelBuffer;
        bitmapBuffer.height = pixelsResponse.height;
        bitmapBuffer.width = pixelsResponse.width;
        bitmapBuffer.rowPitch = pixelsResponse.rowPitch;

        std::wstringstream ss;
        ss << imageSlot + 1 << L'/' << totalImages << L"  " << bitmapBuffer.width << L" x " << bitmapBuffer.height << L" x " << bitmapBuffer.bitsPerPixel << L" BPP";
         
        fWindow.GetImageControl().GetImageList().SetImage({ imageSlot, ss.str(), std::make_shared<BitmapSharedPtr::element_type>(bitmapBuffer) });
    }

    void TestApp::OnContextMenuTimer()
    {
        fContextMenuTimer.SetInterval(0);
        auto pos = Win32Helper::GetMouseCursorPosition();
        auto chosenItem = fContextMenu->Show(pos.x , pos.y);

        if (chosenItem != nullptr)
        {
            CommandManager::CommandClientRequest request;
            request.commandName = chosenItem->command;
            request.args = chosenItem->args;
            ExecuteUserCommand(request);
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

            for (int i = 0; i < subImages.size(); i++)
            {
                auto& currentSubImage = subImages[i];
                AddImageToControl(currentSubImage, static_cast<uint16_t>(i + 1), static_cast<uint16_t>(totalImages));
            }
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
        ResultCode result = file->Load(loadOptions);
        using namespace  std::string_literals;
        switch (result)
        {
        case ResultCode::RC_Success:
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
        UpdateTitle();
        UpdateStatusBar();
        fVirtualStatusBar.SetText("imageDescription", fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescription());
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
    
    void TestApp::ReloadFileInFolder()
    {
        if (IsOpenedImageIsAFile())
            LoadFileInFolder(GetOpenedFileName());
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
        fListFiles.clear();
        fCurrentFileIndex = FileIndexStart;

        std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();


        std::string fileTypesAnsi;
        OIVCommands::GetKnownFileTypes(fileTypesAnsi);
         
        std::wstring fileTypes = LLUtils::StringUtility::ToWString(fileTypesAnsi);

        LLUtils::FileSystemHelper::FindFiles(fListFiles, absoluteFolderPath, fileTypes, false, false);


        UpdateOpenedFileIndex();
        UpdateUIFileIndex();
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
            fMutexWindowCreation.lock();
            // if initial file is provided, load asynchronously.
            asyncResult = async(launch::async, &TestApp::LoadFile, this, filePath, false);
        }
        
        // initialize the windowing system of the window
        fWindow.Create();
        fWindow.SetMenuChar(false);
        fWindow.ShowStatusBar(false);
        fWindow.EnableDragAndDrop(true);
		// Set canvas background the same color as in the renderer for flicker free startup.
		//TODO: fix resize and disable background erasure of top level windows.
		fWindow.SetBackgroundColor(LLUtils::Color(45, 45, 48));
		fWindow.GetCanvasWindow().SetBackgroundColor(LLUtils::Color(45, 45, 48));

        fWindow.SetDoubleClickMode(OIV::Win32::DoubleClickMode::Default);
        {
            using namespace OIV::Win32;
            fWindow.SetWindowStyles(WindowStyle::ResizableBorder | WindowStyle::MaximizeButton | WindowStyle::MinimizeButton, true);
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
        fTimerNoActiveZoom.SetCallback([this]()
            {
                fTimerNoActiveZoom.SetInterval(0);
                fImageState.SetResample(true);
                fImageState.Refresh();
                fRefreshOperation.Queue();
            }
        );
        
        OIVCommands::Init(fWindow.GetCanvasHandle());

        // Update oiv lib client size
        UpdateWindowSize();

        // Wait for initial file to finish loading
        if (asyncResult.valid())
        {
            asyncResult.wait();
            fIsInitialLoad = asyncResult.get();
        }

        // If there is no initial file or the file has failed to load, show the window now, otherwise show the window after 
        // the image has rendered completely at the method FinalizeImageLoad.
        if (fIsInitialLoad == false)
            fWindow.SetVisible(true);
        
        //If initial file is provided but doesn't exist
        if (isInitialFileProvided && !isInitialFileExists)
        {
            using namespace  std::string_literals;
            SetUserMessage(L"Can not load the file: "s + filePath + L", it doesn't exist"s);
        }

        fRefreshOperation.End(fIsInitialLoad == false);
    }

    void TestApp::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        int index = ImageSelectionChangeArgs.imageIndex - 1;
        
        auto image = index == -1 ? fImageState.GetOpenedImage() : fImageState.GetOpenedImage()->GetSubImages()[index];
        fImageState.SetImageChainRoot(image);
        fRefreshOperation.Begin();
        RefreshImage();
        FitToClientAreaAndCenter();
        fRefreshOperation.End();
    }

    void TestApp::PostInitOperations()
    {
			
        // load settings
        fSettings.Load();
        fSettings.Save();

        

        fTimerTopMostRetention.SetTargetWindow(fWindow.GetHandle());
        fTimerTopMostRetention.SetCallback([this]()
        {
            ProcessTopMost();
        }
        );
  

        fTimerSlideShow.SetTargetWindow(fWindow.GetHandle());
        fTimerSlideShow.SetCallback([this]()
        {
            JumpFiles(1);
        }
        );

     
        
        
        fDoubleTap.callback = [this]()
        {
            fWindow.SetAlwaysOnTop(true);
            fTopMostCounter = 3;
            SetTopMostUserMesage();
            fTimerTopMostRetention.SetInterval(1000);
        };

        //If a file has been succesfuly loaded, index all the file in the folder
		if (IsOpenedImageIsAFile())
		{
			LoadFileInFolder(GetOpenedFileName());
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
        OIV_CMD_ColorExposure_Request exposure = fLastColorExposure;
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
        Win32Helper::MessageLoop();
    }

    void TestApp::UpdateUIFileIndex()
    {
        std::wstringstream ss;
        ss << L"File " << (fCurrentFileIndex == FileIndexStart ? 
            0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
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
            UpdateUIFileIndex();
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
            using namespace OIV::Win32;
            fWindow.SetWindowStyles(WindowStyle::ResizableBorder | WindowStyle::MaximizeButton | WindowStyle::MinimizeButton, fShowBorders);
        }
        
    }

    void TestApp::ToggleSlideShow()
    {
        if (fTimerSlideShow.GetInterval() > 0)
            fTimerSlideShow.SetInterval(0);
        else
            fTimerSlideShow.SetInterval(3000);
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

    bool TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
        KeyCombination keyCombination = KeyCombination::FromVirtualKey(static_cast<uint32_t>(evnt->message.wParam),
            static_cast<uint32_t>(evnt->message.lParam));
        const BindingElement& bindings = fKeyBindings.GetBinding(keyCombination);
        if (bindings.command.empty() == false
            && ExecuteUserCommand({ bindings.commandDescription, bindings.command, bindings.arguments }))
                return true; // return if operation has been handled.
        
        return false;
    }

    void TestApp::SetOffset(LLUtils::PointF64 offset)
    {
        fImageState.SetOffset(ResolveOffset(offset));
        fPreserveImageSpaceSelection.Queue();
        fRefreshOperation.Queue();
        fIsOffsetLocked = false;
        fIsLockFitToScreen = false;
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
        if (fImageState.GetOpenedImage() != nullptr)
        {
            CommandManager::CommandClientRequest request;
            request.description = "Zoom";
            request.args = "val=" + std::to_string(amount) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY);
            request.commandName = "cmd_zoom";
            ExecuteUserCommand(request);
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
        using namespace LLUtils;
        SIZE clientSize = fWindow.GetCanvasSize();
        PointF64 ratio = PointF64(clientSize.cx, clientSize.cy) / GetImageSize(ImageSizeType::Transformed);
        double zoom = std::min(ratio.x, ratio.y);
        fRefreshOperation.Begin();
        SetZoomInternal(zoom, -1, -1);
        Center();
        fIsLockFitToScreen = true;
        fRefreshOperation.End();
    }
    
    LLUtils::PointF64 TestApp::GetImageSize(ImageSizeType imageSizeType)
    {
        using namespace LLUtils;
        switch (imageSizeType)
        {
        case ImageSizeType::Original:
            return  fImageState.GetWorkingImageChain().Get(ImageChainStage::SourceImage) != nullptr ? PointF64(fImageState.GetWorkingImageChain().Get(ImageChainStage::SourceImage)->GetDescriptor().Width, fImageState.GetWorkingImageChain().Get(ImageChainStage::SourceImage)->GetDescriptor().Height) : PointF64(0, 0);
        case ImageSizeType::Transformed:
            return PointF64(fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescriptor().Width, fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescriptor().Height);
        case ImageSizeType::Visible:
            return fImageState.GetVisibleSize();

        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }
   

    void TestApp::UpdateUIZoom()
    {
        std::wstringstream ss;
        ss << L"Scale: " << std::fixed << std::setprecision(2);
        if (GetScale() >= 1)
            ss << "x" << GetScale();
        else
            ss << "1/" << 1 / GetScale();

        fWindow.SetStatusBarText(ss.str(), 4, 0);
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

    void TestApp::SetZoomInternal(double zoomValue, int clientX, int clientY)
    {
        using namespace LLUtils;

        const double MinImagePixelsInSmallAxis = 150.0;
        const double MaxPixelSize = 30.0;


        //Apply zoom limits only if zoom is not bound to the client window
        if (fIsLockFitToScreen == false)
        {
            //We want to keep the image at least the size of 'MinImagePixelsInSmallAxis' pixels in the smallest axis.
            PointF64 minimumZoom = MinImagePixelsInSmallAxis / GetImageSize(ImageSizeType::Transformed);
            double minimum = std::min(std::max(minimumZoom.x, minimumZoom.y),1.0);
            
            zoomValue = std::clamp(zoomValue, minimum, MaxPixelSize);
        }

        //Save image selection before view change
        fPreserveImageSpaceSelection.Begin();

        
        PointI32 clientSize = fWindow.GetCanvasSize();
        PointI32 clientZoomPoint = { clientX, clientY };
        if (clientZoomPoint.x < 0)
            clientZoomPoint.x = clientSize.x / 2;

        if (clientZoomPoint.y < 0)
            clientZoomPoint.y = clientSize.y / 2;


        PointF64 imageZoomPoint = ClientToImage(clientZoomPoint);
        PointF64 offset = (imageZoomPoint / GetImageSize(ImageSizeType::Original)) * (GetScale() - zoomValue) * GetImageSize(ImageSizeType::Original);

        if (fDownScalingTechnique == OIV::DownscalingTechnique::Software)
        {
            fImageState.SetResample(false);
            fTimerNoActiveZoom.SetInterval(0);
            fTimerNoActiveZoom.SetInterval(50);
        }

        fImageState.SetScale(zoomValue);
        
        fRefreshOperation.Begin();

        RefreshImage();
        
        
        SetOffset(GetOffset() + offset);
        fPreserveImageSpaceSelection.End();
        
        fRefreshOperation.End();

        UpdateCanvasSize();
        UpdateUIZoom();

        fIsLockFitToScreen = false;
    }

    double TestApp::GetScale() const
    {
        return fImageState.GetScale().x;
    }

    void TestApp::UpdateCanvasSize()
    {
        if (fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed) != nullptr)
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
    }



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

        if (fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed) != nullptr)
        {
            using namespace LLUtils;
            PointF64 storageImageSpace = ClientToImage(fWindow.GetMousePosition());

            std::wstringstream ss;
            ss << L"Texel: "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.x
                << L" X "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.y;
            fVirtualStatusBar.SetText("texelPos", ss.str());
            
            fWindow.SetStatusBarText(ss.str(), 2, 0);

            PointF64 storageImageSize = GetImageSize(ImageSizeType::Transformed);

            if (!(storageImageSpace.x < 0
                || storageImageSpace.y < 0
                || storageImageSpace.x >= storageImageSize.x
                || storageImageSpace.y >= storageImageSize.y
                ))
            {
                OIV_CMD_TexelInfo_Request texelInfoRequest = { fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescriptor().ImageHandle
           ,static_cast<uint32_t>(storageImageSpace.x)
           ,static_cast<uint32_t>(storageImageSpace.y) };
                OIV_CMD_TexelInfo_Response  texelInfoResponse;

                if (OIVCommands::ExecuteCommand(OIV_CMD_TexelInfo, &texelInfoRequest, &texelInfoResponse) == RC_Success)
                {
                    std::wstring message = OIVHelper::ParseTexelValue(texelInfoResponse);
                    OIVString txt = LLUtils::StringUtility::ConvertString<OIVString>(message);
                    OIVTextImage* texelValue = fLabelManager.GetOrCreateTextLabel("texelValue");

                    fVirtualStatusBar.SetText("texelValue", txt);
                    fVirtualStatusBar.SetOpacity("texelValue",1.0);
                    fRefreshOperation.Queue(); 
                }
                
            }
            else
            {
                fVirtualStatusBar.SetOpacity("texelValue", 0);
                fRefreshOperation.Queue();
            }

        }
    }


    void TestApp::AutoPlaceImage(bool forceCenter)
    {
        fRefreshOperation.Begin();
        if (fIsLockFitToScreen == true)
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
        UpdateCanvasSize();
		AutoPlaceImage();
        auto point = static_cast<LLUtils::PointI32>(fWindow.GetCanvasSize());
        fVirtualStatusBar.ClientSizeChanged(point);
    }

    void TestApp::Center()
    {
		if (IsImageOpen() == true)
		{
			fRefreshOperation.Begin();
			using namespace LLUtils;
			PointF64 offset = (PointF64(fWindow.GetCanvasSize()) - GetImageSize(ImageSizeType::Visible)) / 2;
			SetOffset(offset);
			fIsOffsetLocked = true;
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
        const Serialization::UserSettingsData& settings = fSettings.getUserSettings();
        
        offset.x = CalculateOffset(clientSize.x, imageSize.x, offset.x, settings.zoomScrollState.Margins.x);
        offset.y = CalculateOffset(clientSize.y, imageSize.y, offset.y, settings.zoomScrollState.Margins.x);
        return offset;
    }


    void TestApp::TransformImage(OIV_AxisAlignedRotation relativeRotation, OIV_AxisAlignedFlip flip)
    {
	   fRefreshOperation.Begin();
       fImageState.Transform(relativeRotation, flip);
	   AutoPlaceImage(true);
	   RefreshImage();
	   fRefreshOperation.End();
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
        ResultCode result = OIVCommands::CropImage(fImageState.GetVisibleImage()->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);

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

        
        ResultCode result = OIVCommands::CropImage(fImageState.GetWorkingImageChain().Get(ImageChainStage::Deformed)->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);
        
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
    

    LRESULT TestApp::ClientWindwMessage(const Win32::Event* evnt1)
    {
        using namespace Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);
        if (evnt == nullptr)
            return 0;

        const WinMessage & message = evnt->message;

        LRESULT retValue = 0;
        bool defaultProc = true;
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

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
        bool handled = false;

        const Win32::WinMessage & uMsg = evnt->message;
        switch (uMsg.message)
        {
        case WM_SHOWWINDOW:
            if (fIsFirstFrameDisplayed == false)
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
        case Win32::UserMessage::PRIVATE_WM_REFRESH_TIMER:
            PerformRefresh();
            break;
        case Win32::UserMessage::PRIVATE_WN_NOTIFY_USER_MESSAGE:
            SetUserMessage(fLastMessageForMainThread, static_cast<int32_t>(uMsg.wParam));
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
        {

            KeyCombination keyCombination = KeyCombination::FromVirtualKey(static_cast<uint32_t>(evnt->message.wParam),
                static_cast<uint32_t>(evnt->message.lParam));

            bool isAltup = (keyCombination.keycode == KeyCode::LALT || keyCombination.keycode == KeyCode::RIGHTALT || keyCombination.keycode == KeyCode::RALT);

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
			CloseApplication();
        break;

        }

        return handled;
    }
    
	void TestApp::CloseApplication()
	{
		fWindow.Destroy();
	}


    bool TestApp::HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file)
    {
        if (LoadFile(event_ddrag_drop_file->fileName, false))
        {
            LoadFileInFolder(event_ddrag_drop_file->fileName);
        }
        return false;
        
    }

    void TestApp::HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt)
    {
        using namespace Win32;
        
        const RawInputMouseWindow& mouseState = dynamic_cast<MainWindow*>(evnt->window)->GetMouseState();

        const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsRightCatured = mouseState.IsCaptured(MouseState::Button::Right);
        const bool IsLeftCaptured = mouseState.IsCaptured(MouseState::Button::Left);
        const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::Released;
        const bool IsRightReleased = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::Released;
        const bool IsRightPressed = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::Pressed;
        const bool IsLeftPressed = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::Pressed;
        const bool IsMiddlePressed = evnt->GetButtonEvent(MouseState::Button::Middle) == MouseState::EventType::Pressed;
        const bool IsLeftDoubleClick = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::DoublePressed;
        const bool IsRightDoubleClick = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::DoublePressed;
        const bool isMouseUnderCursor = evnt->window->IsUnderMouseCursor();



        using namespace Win32;
        LockMouseToWindowMode LockMode = LockMouseToWindowMode::NoLock;
        
        if (IsLeftPressed && IsRightDown == false && IsRightPressed == false && IsRightCatured == false)
        {
            //Window drag and resize
            if (true
                && Win32Helper::IsKeyPressed(VK_MENU) == false
                && fWindow.IsFullScreen() == false
                )
            {
                if (Win32Helper::IsKeyPressed(VK_CONTROL) == true)
                    LockMode = LockMouseToWindowMode::LockResize;
                else
                    LockMode = LockMouseToWindowMode::LockMove;

            }
        }
        else if (IsLeftReleased)
        {
            LockMode = LockMouseToWindowMode::NoLock;
        }



        fWindow.SetLockMouseToWindowMode(LockMode);



        //Selection rect
        if (Win32Helper::IsKeyPressed(VK_MENU))
        {
            SelectionRect::Operation op = SelectionRect::Operation::NoOp;
            if (IsLeftPressed && isMouseUnderCursor)
                op = SelectionRect::Operation::BeginDrag;
            else if (IsLeftReleased)
                op = SelectionRect::Operation::EndDrag;
            else if (IsLeftCaptured)
                op = SelectionRect::Operation::Drag;

            fSelectionRect.SetSelection(op, evnt->window->GetMousePosition());
            SaveImageSpaceSelection();
        }
      
        
        /*if (IsLeftCaptured == true && evnt->window->IsFullScreen() == false && Win32Helper::IsKeyPressed(VK_MENU))
            evnt->window->Move(evnt->DeltaX, evnt->DeltaY);*/

        if (IsRightCatured == true && fContextMenu->IsVisible() == false)
        {
            if (evnt->DeltaX != 0 || evnt->DeltaY != 0)
                Pan(LLUtils::PointF64( evnt->DeltaX, evnt->DeltaY ));
        }

        LONG wheelDelta = evnt->DeltaWheel;

        if (wheelDelta != 0)
        {
            if (IsRightCatured || isMouseUnderCursor)
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
        
        if (isMouseUnderCursor && evnt->window->IsMouseCursorInClientRect())
        {
            if (IsMiddlePressed)
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
                        fileImage->GetImageProperties().opacity = 0.5;



                        fileImage->GetImageProperties().position = static_cast<LLUtils::PointF64>(static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) - LLUtils::PointI32(fileImage->GetDescriptor().Width, fileImage->GetDescriptor().Height) / 2);
                        fileImage->GetImageProperties().scale = { 1,1 };
                        fileImage->GetImageProperties().opacity = 1.0;

                        fAutoScrollAnchor = std::move(fileImage);
                        fAutoScrollAnchor->Update();
                    }
                }
            }

            if (IsLeftDoubleClick)
            {
                if (fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
                {
                    CancelSelection();
                }
                else
                {
                    ToggleFullScreen(Win32Helper::IsKeyPressed(VK_MENU) ? true : false);
                }
            }

            if (IsRightDoubleClick)
            {
                ExecuteUserCommand({ "Paste image from clipboard","cmd_pasteFromClipboard","" });
            }

            if (IsRightDown)
            {
                if (IsRightPressed)
                {
                    if (fContextMenuTimer.GetInterval() == 0)
                    {
                        fContextMenuTimer.SetInterval(500);
                        fDownPosition = Win32Helper::GetMouseCursorPosition();
                    }
                }
                LLUtils::PointI32  currentPosition = Win32Helper::GetMouseCursorPosition();
                if (currentPosition.DistanceSquared(fDownPosition) > 25)
                    fContextMenuTimer.SetInterval(0);
            }
            else
            {
                fContextMenuTimer.SetInterval(0);
            }
        	
        }

    }

    bool TestApp::HandleClientWindowMessages(const Win32::Event* evnt1)
    {
        using namespace Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
        {
            return ClientWindwMessage(evnt);
        }
        return false;
    }
    
    bool TestApp::HandleMessages(const Win32::Event* evnt1)
    {
        using namespace Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
            return HandleWinMessageEvent(evnt);

        const EventDdragDropFile* dragDropEvent = dynamic_cast<const EventDdragDropFile*>(evnt1);

        if (dragDropEvent != nullptr)
            return HandleFileDragDropEvent(dragDropEvent);

        const EventRawInputMouseStateChanged* rawInputEvent = dynamic_cast<const EventRawInputMouseStateChanged*>(evnt1);

        if (rawInputEvent != nullptr)
            HandleRawInputMouse(rawInputEvent);

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
            textOptions.renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;

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

    bool TestApp::ExecuteUserCommand(const CommandManager::CommandClientRequest& request)
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
        params.renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;
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

