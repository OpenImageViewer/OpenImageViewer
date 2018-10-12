#include <limits>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "TestApp.h"
#include "StringUtility.h"
#include "win32/Win32Window.h"
#include <windows.h>
#include "win32/MonitorInfo.h"
#include <FileSystemHelper.h>

#include <API\functions.h>
#include "Exception.h"
#include "win32/Win32Helper.h"
#include "FileHelper.h"
#include <PlatformUtility.h>
#include "win32/UserMessages.h"
#include "UserSettings.h"
#include "OIVCommands.h"
#include <Rect.h>
#include "Helpers/OIVHelper.h"
#include "Keyboard/KeyCombination.h"
#include "Keyboard/KeyBindings.h"
#include "SelectionRect.h"
#include "Helpers\PhotoshopFinder.h"
#include "API/StringHelper.h"
#include <API/Version.h>
#include "OIVImage\OIVHandleImage.h"
#include "OIVImage\OIVFileImage.h"
#include "OIVImage\OIVRawImage.h"
#include "Helpers\OIVImageHelper.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"


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
            << fixed << setprecision(2) << fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().scale.x * 100.0 << "%)";

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
            PostQuitMessage(0);
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
            result.resValue += fIsSlideShowActive == true ? L"on" : L"off";
        }
        else if (type == "toggleNormalization")
        {
            // Change normalization mode
            fUseRainbowNormalization = !fUseRainbowNormalization;
            
            DisplayImage(fCurrentImageChain.Get(ImageChainStage::Resampled));
            result.resValue = fUseRainbowNormalization ? L"Rainbow normalization" : L"Grayscale normalization";
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
            fWindow.ToggleFullScreen(false);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleMultiFullScreen") //Toggle multi fullscreen
        {
            fWindow.ToggleFullScreen(true);
            fullscreenModeChanged = true;
        }
        else if (type == "toggleresetoffset")
        {
            fResetTransformationMode = static_cast<ResetTransformationMode>((static_cast<int>(fResetTransformationMode) + 1) % static_cast<int>(ResetTransformationMode::Count));
            result.resValue = fResetTransformationMode == ResetTransformationMode::DoNothing ? L"Don't auto reset image state" : L"Auto reset image state";
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
        const int totalColumns = std::ceil(static_cast<double>(fCommandDescription.size()) / MaxLines);
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
        
        requestText.text = OIV_ToOIVString(wmsg);
        requestText.backgroundColor = LLUtils::Color(0_u8, 0, 0, 216).colorValue;
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

        if ( text->Update() == RC_Success)
            fRefreshOperation.Queue();

    }

    void TestApp::CMD_OpenFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& response)
    {
        std::wstring fileName = Win32Helper::OpenFile(fWindow.GetHandle());
        if (fileName.empty() == false)
        {
            LoadFile(fileName, false);
            //response.resValue = "Open file: " + LLUtils::StringUtility::ToAString(fileName);
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
            response.resValue = LLUtils::StringUtility::ToWString(request.description);
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
        PasteFromClipBoard();
        result.resValue = LLUtils::StringUtility::ToWString(request.description);
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
    }


    void HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args)
    {
        using namespace std;
        wstringstream ss;
        std::wstring source = isFromLibrary ? L"OIV library" : L"OIV viewer";
        ss << LLUtils::Exception::ExceptionErrorCodeToString(args.errorCode) + L" exception has occured at " << args.functionName << L" at " << source << L"." << endl;
        ss << "Description: " << args.description << endl;

        if (args.systemErrorMessage.empty() == false)
            ss << "System error: " << args.systemErrorMessage;

        ss << "call stack:" << endl << args.callstack;

        MessageBoxW(nullptr, ss.str().c_str(), L"Unhandled exception has occured.", MB_OK);
        DebugBreak();
    }

    TestApp::TestApp()
        :fRefreshOperation(std::bind(&TestApp::OnRefresh, this))
        , fPreserveImageSpaceSelection(std::bind(&TestApp::OnPreserveSelectionRect, this))
        , fRefreshTimer(std::bind(&TestApp::OnRefreshTimer, this))
        , fSelectionRect(std::bind(&TestApp::OnSelectionRectChanged, this,std::placeholders::_1, std::placeholders::_2))
        , fVirtualStatusBar(&fLabelManager, std::bind(&TestApp::OnLabelRefreshRequest, this))
    {
        EventManager::GetSingleton().MonitorChange.Add(std::bind(&TestApp::OnMonitorChanged, this, std::placeholders::_1));



        OIV_CMD_RegisterCallbacks_Request request;

        request.OnException = [](OIV_Exception_Args args)
        {

            using namespace std;
            //Convert from C to C++
            LLUtils::Exception::EventArgs localArgs;
            localArgs.errorCode = static_cast<LLUtils::Exception::ErrorCode>(args.errorCode);
            localArgs.functionName = args.functionName;
            localArgs.callstack = args.callstack;
            localArgs.description = args.description;
            localArgs.systemErrorMessage = args.systemErrorMessage;

            HandleException(true, localArgs);
        };


        OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &(CmdNull()));

        LLUtils::Exception::OnException.Add([](LLUtils::Exception::EventArgs args)
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
            ,{ "Toggle multi full screen","cmd_view_state","type=toggleMultiFullScreen" ,"Alt+Shift+Enter" }
            ,{ "Borders","cmd_view_state","type=toggleBorders" ,"B" }
            ,{ "Quit","cmd_view_state","type=quit" ,"Escape" }
            ,{ "Grid","cmd_view_state","type=grid" ,"G" }
            ,{ "Slide show","cmd_view_state","type=slideShow" ,"Space" }
            ,{ "Toggle normalization","cmd_view_state","type=toggleNormalization" ,"N" }
            ,{ "Image filter up","cmd_view_state","type=imageFilterUp" ,"Period" }
            ,{ "Image filter down","cmd_view_state","type=imageFilterDown" ,"Comma" }
            ,{ "Toggle reset offset on load","cmd_view_state","type=toggleresetoffset" ,"Backslash" }

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
            }
            else
            {
                //Don't refresh now, restrat refresh timer
                fRefreshTimer.SetDelay((windowTimeInMicroSeconds - microsecSinceLastRefresh) / 1000);
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
        std::wstringstream ss;
        ss << GetOpenedFileName() << L" - OpenImageViewer " << OIV_VERSION_MAJOR << L"." << OIV_VERSION_MINOR;
        HWND handle = GetWindowHandle();
        SetWindowTextW(handle, ss.str().c_str());   
    }


    void TestApp::UpdateStatusBar()
    {
        
        fWindow.SetStatusBarText(fCurrentImageChain.Get(ImageChainStage::Deformed)->GetDescription(), 0, 0);
    }

    void TestApp::DisplayImage(OIVBaseImageSharedPtr& image) 
    {

        image->GetImageProperties().opacity = 1.0;
        if ( image->Update() == RC_Success)
            fRefreshOperation.Queue();


        /*DisplayOptions options = {};
        
        options.fUseRainbowNormalization = fUseRainbowNormalization;
        options.fUseReservedDisplayHandle = true;


        if (image->Display(options) == RC_Success)
            fRefreshOperation.Queue();*/

        //The visible image is different from loaded image.
        //UpdateVisibleImageInfo();
      
    }

    void TestApp::UnloadOpenedImaged()
    {
        fCurrentImageChain.Reset();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void TestApp::DeleteOpenedFile(bool permanently)
    {

        int stringLength = GetOpenedFileName().length();
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
        
        if (fCurrentImageChain.Get(ImageChainStage::Opened) != nullptr)
        {
            fPreviousImageChain = fCurrentImageChain;


            //just hide old display image
            if (fPreviousImageChain.Get(ImageChainStage::Resampled) != nullptr)
            {
                fPreviousImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().opacity = 0.0;
                fPreviousImageChain.Get(ImageChainStage::Resampled)->Update();
            }

            fRefreshOperation.Begin();

            if (fAxisAlignedTransform != OIV_AxisAlignedRotation::AAT_None || fAxisAlignedFlip != OIV_AxisAlignedFlip::AAF_None)
            {

                ImageHandle transformedHandle = ImageHandleNull;
                OIVCommands::TransformImage(fCurrentImageChain.Get(ImageChainStage::Opened)->GetDescriptor().ImageHandle
                    , fAxisAlignedTransform,fAxisAlignedFlip, transformedHandle);
                
                fCurrentImageChain.Get(ImageChainStage::Deformed) = std::make_shared<OIVHandleImage>(transformedHandle);
            }
            else
            {
                fCurrentImageChain.Get(ImageChainStage::Deformed) = fCurrentImageChain.Get(ImageChainStage::Opened);
            }

            fCurrentImageChain.Get(ImageChainStage::Rasterized) = OIVImageHelper::GetRendererCompatibleImage(fCurrentImageChain.Get(ImageChainStage::Deformed), fUseRainbowNormalization);
            fCurrentImageChain.Get(ImageChainStage::Resampled) = fCurrentImageChain.Get(ImageChainStage::Rasterized);
            fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().opacity = 1.0;
            fCurrentImageChain.Get(ImageChainStage::Resampled)->Update();
            AutoPlaceImage();
            fRefreshOperation.End(true);
            fPreviousImageChain.Reset();

            UpdateOpenImageUI();

            //todo: if success reset previous image, if failed keep existing image
        }
    }

    void TestApp::DisplayOpenedFileName()
    {
        if (IsOpenedImageIsAFile())
        {
            std::wstringstream ss;
            ss << L"File: ";
            std::filesystem::path p = GetOpenedFileName();
            ss << p.parent_path().wstring() << L"<textcolor=#ff00ff>" << p.stem().wstring() << L"<textcolor=#00ff00>" << p.extension().wstring();
            SetUserMessage(ss.str());
        }
    }

    void TestApp::FinalizeImageLoad(ResultCode result)
    {
        if (result == RC_Success)
        {
            // Enter this function only from the main thread.
            assert("TestApp::FinalizeImageLoad() can be called only from the main thread" &&
                GetCurrentThreadId() == fMainThreadID);

            //Show new image as soon as possible if not initial file
            if (fIsInitialLoad)
                fRefreshOperation.Begin();

            RefreshImage(); // actual refresh operation won't occur due to the line above.
            UpdateOpenImageUI();
			
            if (fResetTransformationMode == ResetTransformationMode::ResetAll)
                FitToClientAreaAndCenter();

            if (fIsInitialLoad == false)
                UpdateUIFileIndex();

            fRefreshOperation.Begin();
            UnloadWelcomeMessage();
            DisplayOpenedFileName();
            fRefreshOperation.End();

            if (fIsInitialLoad)
                fRefreshOperation.End(false);//Don't refresh on initial file, wait for WM_SIZE
            
            
        }

        if (fIsInitialLoad == true)
        {
            fWindow.SetVisible(true);
            fIsInitialLoad = false;
        }
    }

    void TestApp::FinalizeImageLoadThreadSafe(ResultCode result)
    {
        if (GetCurrentThreadId() == fMainThreadID)
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
        std::shared_ptr<OIVFileImage> file = std::make_shared<OIVFileImage>(filePath);
        FileLoadOptions loadOptions;
        loadOptions.onlyRegisteredExtension = onlyRegisteredExtension;
        ResultCode result = file->Load(loadOptions);
        fCurrentImageChain.Get(ImageChainStage::Opened) = file;
        //Save last display image so it won't be released.
        //TODO: add dirty mode.
        /*fPreviousImageChain.Get(ImageChainStage::Resampled) = fCurrentImageChain.Get(ImageChainStage::Resampled);
        fCurrentImageChain.Get(ImageChainStage::Resampled) = OIVImageHelper::GetRendererCompatibleImage(fCurrentImageChain.Get(ImageChainStage::Opening),fUseRainbowNormalization);*/
        FinalizeImageLoadThreadSafe(result);
        return result == RC_Success;
    }

    void TestApp::UpdateOpenImageUI()
    {
        UpdateTitle();
        UpdateStatusBar();
        fVirtualStatusBar.SetText("imageDescription", fCurrentImageChain.Get(ImageChainStage::Deformed)->GetDescription());
    }


    const std::wstring& TestApp::GetOpenedFileName() const
    {
        static const std::wstring emptyString;
        std::shared_ptr<OIVFileImage> file = std::dynamic_pointer_cast<OIVFileImage>(fCurrentImageChain.Get(ImageChainStage::Opened));
        return file != nullptr ? file->GetFileName() : emptyString;
    }

    bool TestApp::IsOpenedImageIsAFile() const
    {
        return fCurrentImageChain.Get(ImageChainStage::Opened) != nullptr && fCurrentImageChain.Get(ImageChainStage::Opened)->GetDescriptor().Source == ImageSource::File;
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
        using namespace std::experimental::filesystem;
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
            const double PI = 3.14159265358979323846;
            double rad = atan2(-panAmount.y, panAmount.x);
            double deg = (rad * 180) / PI + 180;
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

        const bool isInitialFile = filePath.empty() == false && filesystem::exists(filePath);
        
        future <bool> asyncResult;
        
        if (isInitialFile == true)
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
        fWindow.SetEraseBackground(false);
        fWindow.SetDoubleClickMode(OIV::Win32::DoubleClickMode::Default);
        {
            using namespace OIV::Win32;
            fWindow.SetWindowStyles(WindowStyle::ResizableBorder | WindowStyle::MaximizeButton | WindowStyle::MinimizeButton, true);
        }
    
        AutoScroll::CreateParams params = { fWindow.GetHandle(),Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL, std::bind(&TestApp::OnScroll, this, std::placeholders::_1) };
        fAutoScroll = std::make_unique<AutoScroll>(params);


        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));


        if (isInitialFile == true)
            fMutexWindowCreation.unlock();
        
        
        OIVCommands::Init(fWindow.GetCanvasHandle());

        fVirtualStatusBar.Add("texelValue");
        fVirtualStatusBar.Add("imageDescription");
        fVirtualStatusBar.SetOpacity("imageDescription", 1.0);
        fVirtualStatusBar.Add("texelPos");
        fVirtualStatusBar.SetOpacity("texelPos", 1.0);


        fIsInitialLoad = isInitialFile;

        // Update oiv lib client size
        UpdateWindowSize();

        // Wait for initial file to finish loading
        if (asyncResult.valid())
            asyncResult.wait();

        //If there is no initial file, perform post init operations at the beginning
        if (fIsInitialLoad == false)
            fWindow.SetVisible(true);
        
            
    }

    void TestApp::PostInitOperations()
    {
        // load settings
        fSettings.Load();
        fSettings.Save();

        //If a file has been succesfuly loaded, index all the file in the folder
        if (IsOpenedImageIsAFile())
            LoadFileInFolder(GetOpenedFileName());
        else
            ShowWelcomeMessage();

        AddCommandsAndKeyBindings();
    }

    void TestApp::Destroy()
    {
        // destroy OIV resources before destroying OIV.
        fPreviousImageChain.Reset();
        fCurrentImageChain.Reset();
        fLabelManager.RemoveAll();
        // Destroy OIV when window is closed.
        OIVCommands::ExecuteCommand(OIV_CMD_Destroy, &CmdNull(), &CmdNull());
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
        return fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().filterType;
    }

    void TestApp::UpdateExposure()
    {
        OIVCommands::ExecuteCommand(OIV_CMD_ColorExposure, &fColorExposure, &CmdNull());
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

    bool TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return false;

        LLUtils::ListWString::size_type totalFiles = fListFiles.size();
        int fileIndex = fCurrentFileIndex;


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
            
            if (fileIndex < 0 || fileIndex >= totalFiles || fileIndex == fCurrentFileIndex)
                break;
            
            it = fListFiles.begin();
            std::advance(it, fileIndex);
        }
        
        while ((isLoaded = LoadFile(*it, true)) == false);


        if (isLoaded)
        {
            assert(fileIndex >= 0 && fileIndex < totalFiles);
            fCurrentFileIndex = fileIndex;
            UpdateUIFileIndex();
        }
        return isLoaded;
    }
    
    void TestApp::ToggleFullScreen()
    {
        fWindow.ToggleFullScreen(Win32Helper::IsKeyPressed(VK_MENU) ? true : false);
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
        HWND hwnd = GetWindowHandle();
        
        if (fIsSlideShowActive == false)
            SetTimer(hwnd, cTimerID, 3000, nullptr);
        else
            KillTimer(hwnd, cTimerID);
        fIsSlideShowActive = !fIsSlideShowActive;
        
    }

    void TestApp::SetFilterLevel(OIV_Filter_type filterType)
    {
        fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().filterType =  std::clamp(filterType
            , FT_None, static_cast<OIV_Filter_type>( FT_Count - 1));

        fCurrentImageChain.Get(ImageChainStage::Resampled)->Update();
        fRefreshOperation.Queue();
    }

    void TestApp::ToggleGrid()
    {
        CmdRequestTexelGrid grid;
        fIsGridEnabled = !fIsGridEnabled;
        grid.gridSize = fIsGridEnabled ? 1.0 : 0.0;
        if (OIVCommands::ExecuteCommand(CE_TexelGrid, &grid, &CmdNull()) == RC_Success)
        {
            fRefreshOperation.Queue();
        }
        
    }

    bool TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
        const BindingElement& bindings = fKeyBindings.GetBinding(KeyCombination::FromVirtualKey(evnt->message.wParam, evnt->message.lParam));
        if (bindings.command.empty() == false
            && ExecuteUserCommand({ bindings.commandDescription, bindings.command, bindings.arguments }))
                return true; // return if operation has been handled.
        
        return false;
    }

    void TestApp::SetOffset(LLUtils::PointF64 offset)
    {
        fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().position = ResolveOffset(offset);
        fCurrentImageChain.Get(ImageChainStage::Resampled)->Update();
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
        using namespace LLUtils;
        SetOffset(panAmount + fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().position);
    }

    void TestApp::Zoom(double amount, int zoomX , int zoomY )
    {
        CommandManager::CommandClientRequest request;
        request.description = "Zoom";
        request.args = "val=" + std::to_string(amount) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY);
        request.commandName = "cmd_zoom";
        ExecuteUserCommand(request); 
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
            return fCurrentImageChain.Get(ImageChainStage::Opened) != nullptr ? PointF64(fCurrentImageChain.Get(ImageChainStage::Opened)->GetDescriptor().Width, fCurrentImageChain.Get(ImageChainStage::Opened)->GetDescriptor().Height) : PointF64(0, 0);
        case ImageSizeType::Transformed:
            return PointF64(fCurrentImageChain.Get(ImageChainStage::Deformed)->GetDescriptor().Width, fCurrentImageChain.Get(ImageChainStage::Deformed)->GetDescriptor().Height);
        case ImageSizeType::Visible:
            return PointF64(fCurrentImageChain.Get(ImageChainStage::Resampled)->GetDescriptor().Width, fCurrentImageChain.Get(ImageChainStage::Resampled)->GetDescriptor().Height) * GetScale();
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

        PointF64 zoomPoint;
        PointF64 clientSize = static_cast<PointF64>(fWindow.GetCanvasSize());
        
        if (clientX < 0)
            clientX = clientSize.x / 2.0;

        if (clientY < 0 )
            clientY = clientSize.y / 2.0;

        zoomPoint = ClientToImage(PointI32(clientX, clientY));
        PointF64 offset = (zoomPoint / GetImageSize(ImageSizeType::Original)) * (GetScale() - zoomValue) * GetImageSize(ImageSizeType::Original);
        fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().scale = zoomValue;
        fCurrentImageChain.Get(ImageChainStage::Resampled)->Update();

        fRefreshOperation.Begin();
        
        SetOffset(GetOffset() + offset);
        fPreserveImageSpaceSelection.End();
        
        fRefreshOperation.End();

        UpdateCanvasSize();
        UpdateUIZoom();

        fIsLockFitToScreen = false;
    }

    double TestApp::GetScale() const
    {
        if (fCurrentImageChain.Get(ImageChainStage::Resampled) != nullptr)
            return fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().scale.x;
        else
            return 1.0;
    }

    void TestApp::UpdateCanvasSize()
    {
        if (fCurrentImageChain.Get(ImageChainStage::Opened) != nullptr)
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
        return fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().position;
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

        if (fCurrentImageChain.Get(ImageChainStage::Deformed) != nullptr)
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
                OIV_CMD_TexelInfo_Request texelInfoRequest = { fCurrentImageChain.Get(ImageChainStage::Deformed)->GetDescriptor().ImageHandle
           ,static_cast<uint32_t>(storageImageSpace.x)
           ,static_cast<uint32_t>(storageImageSpace.y) };
                OIV_CMD_TexelInfo_Response  texelInfoResponse;

                if (OIVCommands::ExecuteCommand(OIV_CMD_TexelInfo, &texelInfoRequest, &texelInfoResponse) == RC_Success)
                {
                    std::wstring message = OIVHelper::ParseTexelValue(texelInfoResponse);
                    OIVString txt = OIV_ToOIVString(message);
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


    void TestApp::AutoPlaceImage()
    {
        fRefreshOperation.Begin();
        if (fIsLockFitToScreen == true)
            FitToClientAreaAndCenter();

        else if (fIsOffsetLocked == true)
            Center();
        fRefreshOperation.End();
    }

    void TestApp::UpdateWindowSize()
    {
        SIZE size = fWindow.GetCanvasSize();
        OIVCommands::ExecuteCommand(CMD_SetClientSize,
            &CmdSetClientSizeRequest{ static_cast<uint16_t>(size.cx),
            static_cast<uint16_t>(size.cy) }, &CmdNull());
        UpdateCanvasSize();
		AutoPlaceImage();
        fVirtualStatusBar.ClientSizeChanged(static_cast<LLUtils::PointI32>( fWindow.GetCanvasSize()));
    }

    void TestApp::Center()
    {
        using namespace LLUtils;
        PointF64 offset = (PointF64(fWindow.GetCanvasSize()) - GetImageSize(ImageSizeType::Visible)) / 2;
        SetOffset(offset);
        fIsOffsetLocked = true;
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

        const bool isHorizontalFlip = (int)(fAxisAlignedFlip & static_cast<int>(OIV_AxisAlignedFlip::AAF_Horizontal)) != 0;
        const bool isVerticalFlip = (int)(fAxisAlignedFlip & static_cast<int>(OIV_AxisAlignedFlip::AAF_Vertical)) != 0;
        const bool isFlip = isVerticalFlip || isHorizontalFlip;

        // the two options to manage axes aligned transofrmation are either
        //1. modify the original image so transformation would cumulative - not the case here.
        //2. preserve the original image and add compute the desired transformation - the case here.
        // for simplicity the code accepts only 90 degrees rotations. 

        if (relativeRotation != OIV_AxisAlignedRotation::AAT_None)
        {
            if (relativeRotation != AAT_Rotate90CW && relativeRotation != AAT_Rotate90CCW)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "Rotating image is currently limited to 90 degrees in CW/CCW");

            fAxisAlignedTransform = static_cast<OIV_AxisAlignedRotation>((static_cast<int>(relativeRotation + fAxisAlignedTransform) % 4));
            //If switching axes by rotating 90 degrees and flip is applied, then flip the flip axes.
            if (isFlip == true)
                fAxisAlignedFlip = static_cast<OIV_AxisAlignedFlip>(static_cast<int>(fAxisAlignedFlip) ^ static_cast<int>(3));
        }

        fAxisAlignedFlip = static_cast<OIV_AxisAlignedFlip>(static_cast<int>(fAxisAlignedFlip) ^ static_cast<int>(flip));

       RefreshImage();
    }

    void TestApp::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat)
    {
        fCurrentImageChain.Get(ImageChainStage::Opened).reset();
        std::shared_ptr<OIVRawImage>  rawImage = std::make_shared<OIVRawImage>(ImageSource::Clipboard);
        RawBufferParams params;
        params.width = width;
        params.height = height;
        params.rowPitch = rowPitch;
        params.texelFormat = texelFormat;
        params.buffer = buffer;

        ResultCode result = rawImage->Load(params);
        fCurrentImageChain.Get(ImageChainStage::Opened) = rawImage;

        FinalizeImageLoadThreadSafe(result);
    }

    void TestApp::PasteFromClipBoard()
    {

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
                    }
                }

                CloseClipboard();
            }
        }
    }


    void TestApp::CopyVisibleToClipBoard()
    {
        LLUtils::RectI32 imageRectInt = static_cast<LLUtils::RectI32>(ClientToImage(fSelectionRect.GetSelectionRect()));
        ImageHandle croppedHandle = ImageHandleNull;
        ImageHandle clipboardCompatibleHandle = ImageHandleNull;
        ResultCode result = OIVCommands::CropImage(fCurrentImageChain.Get(ImageChainStage::Resampled)->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);

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

        
        ResultCode result = OIVCommands::CropImage(fCurrentImageChain.Get(ImageChainStage::Opened)->GetDescriptor().ImageHandle, imageRectInt, croppedHandle);
        
        if (result == RC_Success)
        {
            std::shared_ptr<OIVHandleImage> handleImage = std::make_shared<OIVHandleImage>(croppedHandle);
            fPreviousImageChain = fCurrentImageChain;
            fCurrentImageChain.Get(ImageChainStage::Opened) = handleImage;
            CancelSelection();
        }

        FinalizeImageLoadThreadSafe(result);

    }


    void TestApp::AfterFirstFrameDisplayed()
    {
        PostInitOperations();
    }
    

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
        bool handled = false;

        const Win32::WinMessage & uMsg = evnt->message;
        switch (uMsg.message)
        {
        case WM_SIZE:
            UpdateWindowSize();
            fRefreshOperation.Queue();
            break;
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
        case WM_TIMER:
            if (uMsg.wParam == cTimerID)
            
                JumpFiles(1);
            else if (uMsg.wParam == cTimerIDHideUserMessage)
                HideUserMessage();
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


        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            handled = handleKeyInput(evnt);
            break;

        case WM_MOUSEMOVE:
            UpdateTexelPos();
            break;
        break;
        }

        return handled;
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
        
        if (IsLeftPressed)
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

        if (IsRightCatured == true)
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
                    fileImage->Load(options);
                    fileImage->GetImageProperties().imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
                    fileImage->GetImageProperties().opacity = 0.5;
                    
                

                    fileImage->GetImageProperties().position = static_cast<LLUtils::PointF64>(static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) - LLUtils::PointI32(fileImage->GetDescriptor().Width, fileImage->GetDescriptor().Height) / 2);
                    fileImage->GetImageProperties().scale = { 1,1 };
                    fileImage->GetImageProperties().opacity = 1.0;

                    fAutoScrollAnchor = std::move(fileImage);
                    fAutoScrollAnchor->Update();
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
                    ToggleFullScreen();
                }
            }
        }

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


    void TestApp::SetUserMessage(const std::wstring& message)
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
        }


        userMessage->GetImageProperties().opacity = 1.0;

        CreateTextParams& textOptions = userMessage->GetTextOptions();
        
        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += message;
        textOptions.text = OIV_ToOIVString(wmsg);

        if (userMessage->Update() == RC_Success)
        {
            fRefreshOperation.Queue();
            const uint32_t delayMessageBeforeHide = std::max<uint32_t>(fMinDelayRemoveMessage, message.length() * fDelayPerCharacter);
            SetTimer(fWindow.GetHandle(), cTimerIDHideUserMessage, delayMessageBeforeHide, nullptr);
        }
    }

    
    
    void TestApp::SetDebugMessage(const std::string& message)
    {
        OIVTextImage* debugMessage =  fLabelManager.GetOrCreateTextLabel("debugLabel");
        

        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += LLUtils::StringUtility::ToWString(message);

        OIVString txt = OIV_ToOIVString(wmsg);
        CreateTextParams& textOptions = debugMessage->GetTextOptions();
        textOptions.text = txt;
        textOptions.backgroundColor = LLUtils::Color(0_u8, 0, 0, 180).colorValue;
        textOptions.fontPath = LabelManager::sFontPath;

        if (debugMessage->Update() == RC_Success)
        {
            fRefreshOperation.Queue();
        }
    }



    void TestApp::HideUserMessage()
    {
        OIVTextImage* userMessage = fLabelManager.GetTextLabel("userMessage");
        OIV_CMD_ImageProperties_Request& imageProperties = userMessage->GetImageProperties();

        if (imageProperties.opacity == 1.0)
        {
            // start exponential fadeout after 'fDelayRemoveMessage' milliseconds.
            SetTimer(fWindow.GetHandle(), cTimerIDHideUserMessage, 5, nullptr);
            imageProperties.opacity = 0.99;
        }

        imageProperties.opacity = imageProperties.opacity * 0.8;
        if (imageProperties.opacity < 0.01)
        {
            imageProperties.opacity = 0;
            KillTimer(fWindow.GetHandle(), cTimerIDHideUserMessage);
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
        OIVString txt = OIV_ToOIVString(wmsg);
        
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
}

