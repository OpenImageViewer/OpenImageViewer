#include <limits>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "TestApp.h"
#include "FileMapping.h"
#include "StringUtility.h"
#include "win32/Win32Window.h"
#include <windows.h>
#include "win32/MonitorInfo.h"

#include <API\functions.h>
#include "win32/Win32Helper.h"
#include "FileHelper.h"
#include "StopWatch.h"
#include <PlatformUtility.h>
#include "win32/UserMessages.h"
#include "UserSettings.h"
#include "Helpers/FileSystemHelper.h"
#include "OIVCommands.h"
#include <Rect.h>
#include "Helpers/OIVHelper.h"
#include "Keyboard/KeyCombination.h"
#include "Keyboard/KeyBindings.h"

namespace OIV
{
    template <class T,class U>
    ResultCode TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return OIV_Execute(command, sizeof(T), request, sizeof(U), response);
    }

    void TestApp::CMD_SetScreenState(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        ListAString keyval = StringUtility::split(request.args, '=');

        
        if (keyval[0] == "type")
        {
            if (keyval[1] == "toggle") // Toggle fullscreen

                fWindow.ToggleFullScreen(false);
            else if (keyval[1] == "togglemulti") //Toggle multi fullscreen
            {
                fWindow.ToggleFullScreen(true);
            }

            switch (fWindow.GetFullScreenState())
            {
            case FSS_MultiScreen:
                result.resValue = "Multi full screen";
                break;
            case FSS_SingleScreen:
                result.resValue = "Full screen";
                break;
            case FSS_Windowed:
                result.resValue = "Windowed";
                break;
            }
        }
    }

    void TestApp::CMD_ToggleKeyBindings(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        if (fKeybindingsHandle != ImageHandleNull)
        {
            OIVCommands::UnloadImage(fKeybindingsHandle);
            fRefreshOperation.Queue();
            fKeybindingsHandle = ImageHandleNull;
            return;
        }

        using namespace std;
        stringstream ss;
        size_t maxLength = 0;
        for (const CommandDesc& desc : fCommandDescription)
            maxLength = std::max(maxLength, desc.description.length());

        for (const CommandDesc& desc : fCommandDescription)
        {
            ss << desc.description;

            size_t currentLength = desc.description.length();
            while (currentLength++ < maxLength)
                ss << " ";

            ss << " - " << desc.keybindings << "\n";

        }

        std::string message = ss.str();
        if (message[message.size() - 1] == '\n')
            message.erase(message.size() - 1);



        OIV_CMD_CreateText_Request requestText;
        OIV_CMD_CreateText_Response responseText;

        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += LLUtils::StringUtility::ToWString(message);

        requestText.text = const_cast<wchar_t*>(wmsg.c_str());
        requestText.backgroundColor = LLUtils::Color(0, 0, 0, 180);
        requestText.fontPath = L"C:\\Windows\\Fonts\\consola.ttf";
        requestText.fontSize = 22;

        if (ExecuteCommand(OIV_CMD_CreateText, &requestText, &responseText) == RC_Success)
        {
            fKeybindingsHandle = responseText.imageHandle;
            OIV_CMD_ImageProperties_Request imageProperties;
            imageProperties.position = { 20,20 };
            imageProperties.filterType = OIV_Filter_type::FT_None;
            imageProperties.imageHandle = responseText.imageHandle;
            imageProperties.imageRenderMode = IRM_Overlay;
            imageProperties.scale = 1.0;
            imageProperties.opacity = 1.0;
 
            if (ExecuteCommand(OIV_CMD_ImageProperties, &imageProperties, &CmdNull()) == RC_Success)
            {
                fRefreshOperation.Queue();
            }
        }
        
    }

    void TestApp::CMD_AxisAlignedTransform(const CommandManager::CommandRequest& request, CommandManager::CommandResult& response)
    {
        std::string args = request.args;
        using namespace LLUtils;
        using namespace std;
        ListAString keyval = StringUtility::split(request.args, '=');

        OIV_AxisAlignedRTransform transform = OIV_AxisAlignedRTransform::AAT_None;

        

        if (keyval[0] == "type")
        {
            if (false);
            else if (keyval[1] == "hflip")
                transform = AAT_FlipHorizontal;
            else if (keyval[1] == "vflip")
                transform = AAT_FlipVertical;
            else if (keyval[1] == "rotatecw")
                transform = AAT_Rotate90CW;
            else if (keyval[1] == "rotateccw")
                transform = AAT_Rotate90CCW;
        }
        if (transform != OIV_AxisAlignedRTransform::AAT_None)
        {
            TransformImage(transform);
            response.resValue = request.description;
        }
       
    }


    void TestApp::CMD_ToggleColorCorrection(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        using namespace LLUtils;
        using namespace std;
        ListAString keyval = StringUtility::split(request.args, '=');
        
        if (ToggleColorCorrection())
            result.resValue = "Reset color correction to previous";
        else
            result.resValue = "Reset color correction to default";
    }



    void TestApp::CMD_ColorCorrection(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result)
    {
        std::string args = request.args;
        using namespace LLUtils;
        using namespace std;
        ListAString props = StringUtility::split(args, ';');

        string type;
        string op;
        string val;

        for (const std::string& pair : props )
        {
            ListAString keyval = StringUtility::split(pair, '=');
            const std::string& key = keyval[0];
            const std::string& value = keyval[1];

            if (key == "type")
                type = value;
            else if (key == "op")
                op = value;
            else if (key == "val")
                val = value;
        }

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
            
            std::stringstream ss;
            
            ss <<"<textcolor=#00ff00>"<< type << "<textcolor=#7672ff>" <<" " << op << " " << val;
            
            if (op == "increase" || op == "decrease")
                ss << "%";
        
            
        ss << "<textcolor=#00ff00> ("<< std::fixed << std::setprecision(0) << newValue * 100 << "%)";
            result.resValue = ss.str();
            UpdateExposure();
        
    }
    
    TestApp::TestApp()
        :fRefreshOperation(std::bind(&TestApp::OnRefresh,this))
    {
        
        fImageProperties.imageHandle = ImageHandleDisplayed;
        fImageProperties.position = 0;
        fImageProperties.filterType = OIV_Filter_type::FT_None;
        fImageProperties.imageRenderMode = OIV_Image_Render_mode::IRM_MainImage;
        fImageProperties.scale = 1.0;
        fImageProperties.opacity = 1.0;


        fUserMessageOverlayProperties.imageHandle = ImageHandleNull;
        fUserMessageOverlayProperties.position = { 20,20 };
        fUserMessageOverlayProperties.filterType = OIV_Filter_type::FT_None;
        fUserMessageOverlayProperties.imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
        fUserMessageOverlayProperties.scale = 1.0;
        fUserMessageOverlayProperties.opacity = 1.0;


      
     }


    void TestApp::AddCommandsAndKeyBindings()
    {


        std::vector<CommandDesc> localDesc

        {
            { "Toggle full screen,","cmd_screen_state","type=toggle" ,"Alt+Enter" }
            ,{ "Toggle multi full screen,","cmd_screen_state","type=togglemulti" ,"Alt+Shift+Enter" }
            ,{ "Increase Gamma","cmd_color_correction","type=gamma;op=add;val=0.05" ,"Q" }
            ,{ "Decrease Gamma","cmd_color_correction","type=gamma;op=subtract;val=0.05" ,"A" }
            ,{ "Increase Exposure","cmd_color_correction","type=exposure;op=add;val=0.05" ,"W" }
            ,{ "Decrease Exposure","cmd_color_correction","type=exposure;op=subtract;val=0.05" ,"S" }
            ,{ "Increase Offset","cmd_color_correction","type=offset;op=add;val=0.01" ,"E" }
            ,{ "Decrease Offset","cmd_color_correction","type=offset;op=subtract;val=0.01" ,"D" }
            ,{ "Increase Saturation","cmd_color_correction","type=saturation;op=add;val=0.05" ,"R" }
            ,{ "Decrease Saturation","cmd_color_correction","type=saturation;op=subtract;val=0.05" ,"F" }
            ,{ "Toggle color correction","cmd_toggle_correction","" ,"Z" }
            ,{ "Toggle key bindings","cmd_toggle_keybindings","" ,"F1" }
            ,{ "Horizontal flip","cmd_axis_aligned_transform","type=hflip" ,"H" }
            ,{ "Vertical flip","cmd_axis_aligned_transform","type=vflip" ,"V" }
            ,{ "Rotate clockwise","cmd_axis_aligned_transform","type=rotatecw" ,"RBracket" }
            ,{ "Rotate counter clockwise","cmd_axis_aligned_transform","type=rotateccw" ,"LBracket" }

        };

        fCommandDescription = localDesc;

        using namespace std;
        using namespace placeholders;
        
        fCommandManager.AddCommand(CommandManager::Command("cmd_color_correction", std::bind(&TestApp::CMD_ColorCorrection, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_screen_state", std::bind(&TestApp::CMD_SetScreenState, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_toggle_correction", std::bind(&TestApp::CMD_ToggleColorCorrection, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_toggle_keybindings", std::bind(&TestApp::CMD_ToggleKeyBindings, this, _1, _2)));
        fCommandManager.AddCommand(CommandManager::Command("cmd_axis_aligned_transform", std::bind(&TestApp::CMD_AxisAlignedTransform, this, _1, _2)));


        for (const CommandDesc& desc : fCommandDescription)
            fKeyBindings.AddBinding(KeyCombination::FromString(desc.keybindings)
                , { desc.description, desc.command,desc.arguments });
    }

    void TestApp::OnRefresh()
    {
        OIVCommands::Refresh();
    }

    HWND TestApp::GetWindowHandle() const
    {
        return fWindow.GetHandle();
    }

    void TestApp::UpdateTitle()
    {
        std::wstringstream ss;
        ss << fOpenedImage.GetName() << L" - OpenImageViewer";
        HWND handle = GetWindowHandle();
        SetWindowTextW(handle, ss.str().c_str());   
    }


    void TestApp::UpdateStatusBar()
    {
        fWindow.SetStatusBarText(fOpenedImage.GetDescription(), 0, 0);
    }

    void TestApp::DisplayImage(ImageDescriptor& descriptor , bool resetScrollState) 
    {
        if (descriptor.imageHandle != ImageHandleNull)
        {
            LLUtils::StopWatch stopWatch(true);

           


            OIVCommands::DisplayImage(descriptor.imageHandle
                , static_cast<OIV_CMD_DisplayImage_Flags>(
                    OIV_CMD_DisplayImage_Flags::DF_ApplyExifTransformation
                    | 0 //(fUpdateWindowOnInitialFileLoad == false ? OIV_CMD_DisplayImage_Flags::DF_RefreshRenderer : 0)
                    | (resetScrollState ? OIV_CMD_DisplayImage_Flags::DF_ResetScrollState : 0))
                , fUseRainbowNormalization ? OIV_PROP_Normalize_Mode::NM_Rainbow : OIV_PROP_Normalize_Mode::NM_Monochrome
            );

            descriptor.displayTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

            fRefreshOperation.Queue();
            UpdateVisibleImageInfo();
        }
    }

    void TestApp::UnloadOpenedImaged()
    {
        OIVCommands::UnloadImage(fOpenedImage.imageHandle);
        OIVCommands::ClearImage();
        fRefreshOperation.Queue();
        SetOpenImage(ImageDescriptor());
    }

    void TestApp::DeleteOpenedFile(bool permanently)
    {

        int stringLength = fOpenedImage.fileName.length();
        std::unique_ptr<wchar_t> buffer = std::unique_ptr<wchar_t>(new wchar_t[stringLength + 2]);

        memcpy(buffer.get(), fOpenedImage.fileName.c_str(), ( stringLength + 1) * sizeof(wchar_t));

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


        int currentIndex = fCurrentFileIndex;
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

    void TestApp::FinalizeImageLoad(ResultCode result)
    {
        if (result == RC_Success)
        {
            // Enter this function only from the main thread.
            assert("TestApp::FinalizeImageLoad() can be called only from the main thread" &&
                GetCurrentThreadId() == fMainThreadID);

            ImageHandle oldImage = fOpenedImage.imageHandle;

            fRefreshOperation.Begin();
            DisplayImage(fImageBeingOpened,true);
            SetOpenImage(fImageBeingOpened);
            FitToClientAreaAndCenter();
            //Don't refresh on initial file, wait for WM_SIZE
            fRefreshOperation.End(!fIsInitialLoad);
            
            fImageBeingOpened = ImageDescriptor();

            if (fIsInitialLoad == false)
                UpdateUIFileIndex();

            //Unload old image
            OIVCommands::UnloadImage(oldImage);
        }

        if (fIsInitialLoad == true)
        {
            fIsInitialLoad = false;
            PostInitOperations();
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
        fImageBeingOpened = ImageDescriptor();
        fImageBeingOpened.fileName = filePath;
        fImageBeingOpened.source = ImageSource::IS_File;
        using namespace LLUtils;
        FileMapping fileMapping(filePath);
        void* buffer = fileMapping.GetBuffer();
        std::size_t size = fileMapping.GetSize();
        std::string extension = StringUtility::ToAString(StringUtility::GetFileExtension(filePath));
        return LoadFileFromBuffer((uint8_t*)buffer, size, extension, onlyRegisteredExtension);
    }

    void TestApp::SetOpenImage(const ImageDescriptor& image_descriptor)
    {
        fOpenedImage = image_descriptor;
        UpdateTitle();
        UpdateStatusBar();
    }

    bool TestApp::LoadFileFromBuffer(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension)
    {
        using namespace LLUtils;
        OIV_CMD_LoadFile_Response loadResponse;
        OIV_CMD_LoadFile_Request loadRequest = {};

        loadRequest.buffer = (void*)buffer;
        loadRequest.length = size;
        std::string fileExtension = extension;
        strcpy_s(loadRequest.extension, OIV_CMD_LoadFile_Request::EXTENSION_SIZE, fileExtension.c_str());
        loadRequest.flags = static_cast<OIV_CMD_LoadFile_Flags>(
              (onlyRegisteredExtension ? OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension : 0)
            | OIV_CMD_LoadFile_Flags::Load_Exif_Data);

        ResultCode result = ExecuteCommand(CommandExecute::OIV_CMD_LoadFile, &loadRequest, &loadResponse);
        if (result == RC_Success)
        {
            fImageBeingOpened.width = loadResponse.width;
            fImageBeingOpened.height = loadResponse.height;
            fImageBeingOpened.loadTime = loadResponse.loadTime;
            fImageBeingOpened.imageHandle = loadResponse.handle;
            fImageBeingOpened.bpp = loadResponse.bpp;
            
        }
        FinalizeImageLoadThreadSafe(result);

        return result == RC_Success;
    }
    
    

    void TestApp::ReloadFileInFolder()
    {
        if (fOpenedImage.source == ImageSource::IS_File)
            LoadFileInFolder(fOpenedImage.GetName());
    }

    void TestApp::UpdateOpenedFileIndex()
    {
        if (fOpenedImage.source == IS_File)
        {
            LLUtils::ListStringIterator it = std::find(fListFiles.begin(), fListFiles.end(), fOpenedImage.fileName);

            if (it != fListFiles.end())
                fCurrentFileIndex = std::distance(fListFiles.begin(), it);
        }
    }

    void TestApp::LoadFileInFolder(std::wstring absoluteFilePath)
    {
        using namespace std::experimental::filesystem;
        fListFiles.clear();
        fCurrentFileIndex = std::numeric_limits<LLUtils::ListWString::size_type>::max();

        std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();

        LLUtils::PlatformUtility::find_files(absoluteFolderPath, fListFiles);

        UpdateOpenedFileIndex();
        UpdateUIFileIndex();
    }

    void TestApp::OnScroll(const LLUtils::PointF64& panAmount)
    {
        Pan(panAmount);
    }


    void TestApp::Init(std::wstring relativeFilePath)
    {
        using namespace std;
        using namespace placeholders;
        using namespace experimental;
        
        wstring filePath = FileSystemHelper::ResolveFullPath(relativeFilePath);

        const bool isInitialFile = filePath.empty() == false && filesystem::exists(filePath);
        
        future <bool> asyncResult;
        
        
        if (isInitialFile == true)
        {
            fMutexWindowCreation.lock();
            // if initial file is provided, load asynchronously.
            asyncResult = async(launch::async, &TestApp::LoadFile, this, filePath, false);
        }
        
        // initialize the windowing system of the window
        fWindow.Create(GetModuleHandle(nullptr), SW_HIDE);
        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));


        if (isInitialFile == true)
            fMutexWindowCreation.unlock();
        
        
        // Init OIV renderer
        CmdDataInit init;
        init.parentHandle = reinterpret_cast<std::size_t>(fWindow.GetHandleClient());
        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());

        // Update the window size manually since the window won't receive WM_SIZE till it's visible.
        fWindow.RefreshWindow();

        fIsInitialLoad = isInitialFile;

        // Update oiv lib client size
        UpdateWindowSize();

        // Wait for initial file to finish loading
        if (asyncResult.valid())
            asyncResult.wait();

        //If there is no initial file, perform post init operations at the beginning
        if (isInitialFile == false)
            PostInitOperations();

    }

    void TestApp::PostInitOperations()
    {
        fWindow.Show(true);
        // load settings
        fSettings.Load();

        //If a file has been succesfuly loaded, index all the file in the folder
        if (fOpenedImage.source ==  IS_File)
            LoadFileInFolder(fOpenedImage.fileName);

        AddCommandsAndKeyBindings();
    }

    void TestApp::Destroy()
    {
        // Destroy OIV when window is closed.
        ExecuteCommand(OIV_CMD_Destroy, &CmdNull(), &CmdNull());
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
        return fImageProperties.filterType;
    }

    void TestApp::UpdateExposure()
    {
        ExecuteCommand(OIV_CMD_ColorExposure, &fColorExposure, &CmdNull());
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
        ss << L"File " << (fCurrentFileIndex == std::numeric_limits<LLUtils::ListWString::size_type>::max() ? 
            0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
    }

    bool TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return false;

        LLUtils::ListWString::size_type totalFiles = fListFiles.size();
        int32_t fileIndex = static_cast<int32_t>(fCurrentFileIndex);


        int sign;
        if (step == std::numeric_limits<int>::max())
        {
            // Last
            fileIndex = static_cast<int32_t>(fListFiles.size());
            sign = -1;
        }
        else if (step == std::numeric_limits<int>::min())
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
        LLUtils::ListStringIterator it;

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
            fCurrentFileIndex = static_cast<LLUtils::ListWString::size_type>(fileIndex);
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
        static bool showBorders = true;
        showBorders = !showBorders;
        fWindow.ShowBorders(showBorders);
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
        fImageProperties.filterType = LLUtils::Utility::Clamp(filterType
            , FT_None, static_cast<OIV_Filter_type>( FT_Count - 1));
        UpdateImageProperties();
        fRefreshOperation.Queue();
    }

    void TestApp::ToggleGrid()
    {
        CmdRequestTexelGrid grid;
        fIsGridEnabled = !fIsGridEnabled;
        grid.gridSize = fIsGridEnabled ? 1.0 : 0.0;
        if (ExecuteCommand(CE_TexelGrid, &grid, &CmdNull()) == RC_Success)
        {
            fRefreshOperation.Queue();
        }
        
    }

    bool TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
        const BindingElement& bindings = fKeyBindings.GetBinding(KeyCombination::FromVirtualKey(evnt->message.wParam));


        if (bindings.command.empty() == false
            && ExecuteUserCommand({ bindings.commandDescription, bindings.command, bindings.arguments }))
                return true; // return if operation has been handled.
        
        bool handled = true;
        bool IsAlt =  (GetKeyState(VK_MENU) & static_cast<USHORT>(0x8000)) != 0;
        bool IsControl = (GetKeyState(VK_CONTROL) & static_cast<USHORT>(0x8000)) != 0;
        bool IsShift = (GetKeyState(VK_SHIFT) & static_cast<USHORT>(0x8000)) != 0;

        switch (evnt->message.wParam)
        {
        case 'N':
            if (IsControl == true)
            {
                //Open new window
                ShellExecute(nullptr, L"open", LLUtils::PlatformUtility::GetExePath().c_str(), fOpenedImage.fileName.c_str(), nullptr, SW_SHOWDEFAULT);

            }
            else
            {
                // Change normalization mode
                fUseRainbowNormalization = !fUseRainbowNormalization;
                DisplayImage(fOpenedImage, false);
            }
            break;
        case 'C':
            if (IsControl == true)
            {
                if (IsShift && fOpenedImage.source == IS_File)
                    LLUtils::PlatformUtility::CopyTextToClipBoard(fOpenedImage.fileName);
                else
                    CopyVisibleToClipBoard();
            }
            else
                CropVisibleImage();
            break;
        case 'V':
            if (IsControl == true)
                PasteFromClipBoard();
                break;
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case VK_NEXT:
                JumpFiles(1);
            break;

        case VK_DOWN:
        case VK_RIGHT:
            JumpFiles(1);
            break;
        case VK_UP:
        case VK_LEFT:
            JumpFiles(-1);
            break;
        case VK_PRIOR:
                JumpFiles(1);
            break;
        case VK_HOME:
            JumpFiles(std::numeric_limits<int>::min());
            break;
        case VK_END:
            JumpFiles(std::numeric_limits<int>::max());
            break;
        case VK_SPACE:
            ToggleSlideShow();
            break;
        case 'B':
            ToggleBorders();
            break;
        case VK_OEM_PERIOD:
            SetFilterLevel(static_cast<OIV_Filter_type>( static_cast<int>(GetFilterType()) + 1));
            break;
        case VK_OEM_COMMA:
            SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(GetFilterType()) - 1));
            break;
        case VK_NUMPAD8:
            Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(fKeyboardPanSpeed) ));
            break;
        case VK_NUMPAD2:
            Pan(LLUtils::PointF64(0, fAdaptivePanUpDown.Add(-fKeyboardPanSpeed) ));
            break;
        case VK_NUMPAD4:
            Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(fKeyboardPanSpeed), 0));
            break;
        case VK_NUMPAD6:
            Pan(LLUtils::PointF64(fAdaptivePanLeftRight.Add(-fKeyboardPanSpeed), 0));
            break;
        case VK_ADD:
            Zoom(fKeyboardZoomSpeed, -1, -1);
            break;
        case VK_SUBTRACT:
            Zoom(-fKeyboardZoomSpeed, -1, -1);
            break;
        case VK_NUMPAD5:
            Center();
            break;
        case VK_MULTIPLY:
            SetOriginalSize();
            break;
        case VK_DIVIDE:
            FitToClientAreaAndCenter();
            break;
        case VK_DELETE:
         
             DeleteOpenedFile(IsShift);
            break;
        case 'G':
            ToggleGrid();
            break;
        case 'P':
        {
            std::wstring command = LR"(c:\Program Files\Adobe\Adobe Photoshop CC 2017\Photoshop.exe)";
            ShellExecute(nullptr, L"open", command.c_str(),  fOpenedImage.fileName.c_str(), nullptr, SW_SHOWDEFAULT);
        }
        default:
            handled = false;
        break;

        }
    }

    void TestApp::SetOffset(LLUtils::PointF64 offset)
    {
        fImageProperties.position = ResolveOffset(offset);
        UpdateImageProperties();
        fRefreshOperation.Queue();
        fIsOffsetLocked = false;
        fIsLockFitToScreen = false;
    }

    void TestApp::SetOriginalSize()
    {
        SetZoom(1.0, -1, -1);
        Center();
    }

    void TestApp::Pan(const LLUtils::PointF64& panAmount )
    {
        using namespace LLUtils;
        SetOffset(panAmount + fImageProperties.position);
    }

    void TestApp::Zoom(double amount, int zoomX , int zoomY )
    {
        const double adaptiveAmount = fAdaptiveZoom.Add(amount);
        const double adjustedAmount = adaptiveAmount > 0 ? GetScale() * (1 + adaptiveAmount) : GetScale() / (1 - adaptiveAmount);
        SetZoom(adjustedAmount, zoomX, zoomY);
    }
    
    void TestApp::FitToClientAreaAndCenter()
    {
        using namespace LLUtils;
        SIZE clientSize = fWindow.GetClientSize();
        PointF64 ratio = PointF64(clientSize.cx, clientSize.cy) / GetImageSize(IST_Transformed);
        double zoom = std::min(ratio.x, ratio.y);
        fRefreshOperation.Begin();
        SetZoom(zoom, -1, -1);
        Center();
        fIsLockFitToScreen = true;
        fRefreshOperation.End();
    }
    
    LLUtils::PointF64 TestApp::GetImageSize(ImageSizeType imageSizeType)
    {
        switch (imageSizeType)
        {
        case IST_Original:
            return LLUtils::PointF64(fOpenedImage.width, fOpenedImage.height);
        case IST_Transformed:
            return LLUtils::PointF64(fVisibleFileInfo.width, fVisibleFileInfo.height);
        case IST_Visible:
            return LLUtils::PointF64(fVisibleFileInfo.width, fVisibleFileInfo.height) * GetScale();
        default:
            throw std::logic_error("Unexpected or corrupted value");
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

    void TestApp::UpdateImageProperties()
    {
        ExecuteCommand(OIV_CMD_ImageProperties, &fImageProperties, &CmdNull());
    }

    void TestApp::SetZoom(double zoomValue, int clientX, int clientY)
    {
        using namespace LLUtils;

        const double MinImagePixelsInSmallAxis = 150.0;
        const double MaxPixelSize = 30.0;


        //Apply zoom limits only if zoom is not bound to the client window
        if (fIsLockFitToScreen == false)
        {
            //We want to keep the image at least the size of 'MinImagePixelsInSmallAxis' pixels in the smallest axis.
            PointF64 minimumZoom = MinImagePixelsInSmallAxis / GetImageSize(ImageSizeType::IST_Transformed);
            double minimum = std::min(std::max(minimumZoom.x, minimumZoom.y),1.0);
            
            zoomValue = Utility::Clamp(zoomValue, minimum, MaxPixelSize);
        }

        PointF64 zoomPoint;
        PointF64 clientSize = static_cast<PointF64>(fWindow.GetClientSize());
        
        if (clientX < 0)
            clientX = clientSize.x / 2.0;

        if (clientY < 0 )
            clientY = clientSize.y / 2.0;

        zoomPoint = ClientToImage(PointI32(clientX, clientY));
        PointF64 offset = (zoomPoint / GetImageSize(IST_Original)) * (GetScale() - zoomValue) * GetImageSize(IST_Original);
        fImageProperties.scale = zoomValue;

        UpdateImageProperties();
        fRefreshOperation.Begin();
        
        SetOffset(GetOffset() + offset);
        
        fRefreshOperation.End();

        UpdateCanvasSize();
        UpdateUIZoom();

        fIsLockFitToScreen = false;
    }

    double TestApp::GetScale() const
    {
        return fImageProperties.scale.x;
    }

    void TestApp::UpdateCanvasSize()
    {
        using namespace  LLUtils;
        PointF64 canvasSize = (PointF64)fWindow.GetClientSize() / GetScale();
            std::wstringstream ss;
            ss << L"Canvas: "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.x
                << L" X "
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.y;
            fWindow.SetStatusBarText(ss.str(), 3, 0);
    }



    LLUtils::PointF64 TestApp::GetOffset() const
    {
        return fImageProperties.position;
    }

    LLUtils::PointF64 TestApp::ClientToImage(LLUtils::PointI32 clientPos) const
    {
        using namespace LLUtils;
        return (static_cast<PointF64>(clientPos) - GetOffset()) / GetScale();
    }

    void TestApp::UpdateTexelPos()
    {
        using namespace LLUtils;
        PointF64 storageImageSpace = ClientToImage(fWindow.GetMousePosition());

        std::wstringstream ss;
        ss << L"Texel: "
            << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.x
            << L" X "
            << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << storageImageSpace.y;
        fWindow.SetStatusBarText(ss.str(), 2, 0);

        PointF64 storageImageSize = GetImageSize(IST_Original);
       
        if (!( storageImageSpace.x < 0 
            || storageImageSpace.y < 0
            || storageImageSpace.x >= storageImageSize.x
            || storageImageSpace.y >= storageImageSize.y
            ))
        {
                 OIV_CMD_TexelInfo_Request texelInfoRequest = {fOpenedImage.imageHandle
            ,static_cast<int32_t>(storageImageSpace.x)
            ,static_cast<int32_t>(storageImageSpace.y)};
            OIV_CMD_TexelInfo_Response  texelInfoResponse;

            if (ExecuteCommand(OIV_CMD_TexelInfo, &texelInfoRequest, &texelInfoResponse) == RC_Success)
            fWindow.SetStatusBarText(OIVHelper::ParseTexelValue(texelInfoResponse), 5, 0);
        }
        else
        {
            fWindow.SetStatusBarText(L"", 5, 0);
        }
    }


    void TestApp::UpdateWindowSize()
    {
        SIZE size = fWindow.GetClientSize();
        ExecuteCommand(CMD_SetClientSize,
            &CmdSetClientSizeRequest{ static_cast<uint16_t>(size.cx),
            static_cast<uint16_t>(size.cy) }, &CmdNull());
        UpdateCanvasSize();

        fRefreshOperation.Begin();

        if (fIsLockFitToScreen == true)
            FitToClientAreaAndCenter();

        else if (fIsOffsetLocked == true)
            Center();

        fRefreshOperation.End();
    }

    void TestApp::Center()
    {
        using namespace LLUtils;
        PointF64 offset = (PointF64(fWindow.GetClientSize()) - GetImageSize(IST_Visible)) / 2;
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
        PointF64 imageSize = GetImageSize(IST_Visible);
        PointF64 clientSize = fWindow.GetClientSize();
        PointF64 offset = static_cast<PointF64>(point);
        const Serialization::UserSettingsData& settings = fSettings.getUserSettings();
        
        offset.x = CalculateOffset(clientSize.x, imageSize.x, offset.x, settings.zoomScrollState.Margins.x);
        offset.y = CalculateOffset(clientSize.y, imageSize.y, offset.y, settings.zoomScrollState.Margins.x);
        return offset;
    }

    void TestApp::TransformImage(OIV_AxisAlignedRTransform transform)
    {
        fRefreshOperation.Begin();
        
        OIVCommands::TransformImage(ImageHandleDisplayed, transform);
        UpdateVisibleImageInfo();
        
        if (fIsLockFitToScreen)
            FitToClientAreaAndCenter();
        else
            Center();

        fRefreshOperation.End();
    }

    void TestApp::UpdateVisibleImageInfo()
    {
        OIV_CMD_QueryImageInfo_Request loadRequest;
        loadRequest.handle = ImageHandleDisplayed;
        ResultCode result = ExecuteCommand(CommandExecute::OIV_CMD_QueryImageInfo, &loadRequest, &fVisibleFileInfo);
    }

    void TestApp::LoadRaw(const uint8_t* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat)
    {
        using namespace LLUtils;
        
        OIV_CMD_LoadRaw_Response loadResponse;
        OIV_CMD_LoadRaw_Request loadRequest = {};
        loadRequest.buffer = const_cast<uint8_t*>(buffer);
        loadRequest.width = width;
        loadRequest.height = height;
        loadRequest.rowPitch = rowPitch;
        loadRequest.texelFormat = texelFormat;
        loadRequest.transformation = OIV_AxisAlignedRTransform::AAT_FlipVertical;
        
        
        ResultCode result = ExecuteCommand(CommandExecute::OIV_CMD_LoadRaw, &loadRequest, &loadResponse);
        if (result == RC_Success)
        {
            fImageBeingOpened = ImageDescriptor();
            fImageBeingOpened.width = loadRequest.width;
            fImageBeingOpened.height = loadRequest.height;
            fImageBeingOpened.loadTime = loadResponse.loadTime;
            fImageBeingOpened.source = ImageSource::IS_Clipboard;
            OIV_Util_GetBPPFromTexelFormat(texelFormat, &fImageBeingOpened.bpp);
            fImageBeingOpened.imageHandle = loadResponse.handle;
        }

        FinalizeImageLoadThreadSafe(result);

        //return success;

    }

    void TestApp::PasteFromClipBoard()
    {

        if (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5))
        {
            if (OpenClipboard(NULL))
            {
                HANDLE hClipboard = GetClipboardData(CF_DIB);

                if (!hClipboard)
                {
                    hClipboard = GetClipboardData(CF_DIBV5);
                }

                if (hClipboard != NULL && hClipboard != INVALID_HANDLE_VALUE)
                {
                    void* dib = GlobalLock(hClipboard);

                    if (dib)
                    {
                        BITMAPINFOHEADER *info = reinterpret_cast<BITMAPINFOHEADER*>(dib);
                        
                        uint32_t imageSize = info->biWidth * info->biHeight * (info->biBitCount / 8);

                        LoadRaw(reinterpret_cast<const uint8_t*>(info + 1)
                                , info->biWidth
                                , info->biHeight
                                , info->biSizeImage / info->biHeight
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
        LLUtils::RectF64 imageRect = { ClientToImage(fSelectionRect.p0), ClientToImage(fSelectionRect.p1) };
        LLUtils::RectI32 imageRectInt = { { (int)imageRect.p0.x,(int)imageRect.p0.y}
            ,{ (int)imageRect.p1.x,(int)imageRect.p1.y } };

        ImageHandle croppedHandle;
        ResultCode result = OIVCommands::CropImage(ImageHandleDisplayed, imageRectInt, croppedHandle);
        if (result == RC_Success)
        {

            struct unloadImage
            {
                ImageHandle handle;
                ~unloadImage()
                {
                    if (handle != ImageHandleNull)
                        OIVCommands::UnloadImage(handle);

                }
            } handle{ croppedHandle };


            //2. Flip the image vertically and convert it to BGRA for the clipboard.
            result = OIVCommands::TransformImage(croppedHandle, OIV_AxisAlignedRTransform::AAT_FlipVertical);
            result = OIVCommands::ConvertImage(croppedHandle, OIV_TexelFormat::TF_I_B8_G8_R8_A8);

            //3. Get image pixel buffer and Copy to clipboard.

            OIV_CMD_GetPixels_Request requestGetPixels;
            OIV_CMD_GetPixels_Response responseGetPixels;

            requestGetPixels.handle = croppedHandle;

            if (ExecuteCommand(OIV_CMD_GetPixels, &requestGetPixels, &responseGetPixels) == RC_Success)
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
                        //Failed setting clipboard data.
                        std::string error = LLUtils::PlatformUtility::GetLastErrorAsString();
                        throw std::logic_error("Unable to set clipboard data.\n" + error);
                    }
                    if (CloseClipboard() == FALSE)
                        throw std::logic_error("Unknown error");
                }
                else
                {
                    throw std::logic_error("could not open clipboard");
                }
            }


        }
    }

    void TestApp::CropVisibleImage()
    {
        LLUtils::RectF64 imageRect = { ClientToImage(fSelectionRect.p0), ClientToImage(fSelectionRect.p1) };


        LLUtils::RectI32 imageRectInt = { { (int)imageRect.p0.x,(int)imageRect.p0.y }
        ,{ (int)imageRect.p1.x,(int)imageRect.p1.y } };

        ImageHandle croppedHandle;
        ResultCode result = OIVCommands::CropImage(ImageHandleDisplayed, imageRectInt, croppedHandle);
        if (result == RC_Success)
        {
            fImageBeingOpened = fOpenedImage;
            fImageBeingOpened.imageHandle = croppedHandle;
            fImageBeingOpened.width = 7;
            fImageBeingOpened.height = 7;

        }

        FinalizeImageLoadThreadSafe(result);

    }

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
        bool handled = false;

        const MSG& uMsg = evnt->message;
        switch (uMsg.message)
        {
        case WM_SIZE:
            UpdateWindowSize();
            fRefreshOperation.Queue();
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
            fAutoScroll.PerformAutoScroll(evnt);
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
        
        const RawInputMouseWindow& mouseState = evnt->window->GetMouseState();

        const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsRightCatured = mouseState.IsCaptured(MouseState::Button::Right);
        const bool IsLeftCaptured = mouseState.IsCaptured(MouseState::Button::Left);
        const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_Released;
        const bool IsRightReleased = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::ET_Released;
        const bool IsRightPressed = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::ET_Pressed;
        const bool IsLeftPressed = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_Pressed;
        const bool IsMiddlePressed = evnt->GetButtonEvent(MouseState::Button::Middle) == MouseState::EventType::ET_Pressed;
        const bool IsLeftDoubleClick = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_DoublePressed;
        const bool IsRightDoubleClick = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::ET_DoublePressed;

        const bool isMouseUnderCursor = evnt->window->IsUnderMouseCursor();
        
        static bool isSelecting = false;

        if (IsLeftPressed == true && Win32Helper::IsKeyPressed(VK_MENU))
        {
            if (fDragStart.x == -1)
            {
                fSelectionRect = { {-1,-1},{-1,-1} };

                ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &fSelectionRect, &CmdNull());
                //Disable selection rect
            }
            fDragStart = evnt->window->GetMousePosition();
            
        }

        else
            if (IsLeftCaptured == true)
            {
                LLUtils::PointI32 dragCurent = evnt->window->GetMousePosition();
                if (fDragStart.x != -1 && isSelecting == true || dragCurent.DistanceSquared(fDragStart) > 225)
                {
                    isSelecting = true;
                    LLUtils::PointI32 p0 = { std::min(dragCurent.x, fDragStart.x), std::min(dragCurent.y, fDragStart.y) };
                    LLUtils::PointI32 p1 = { std::max(dragCurent.x, fDragStart.x), std::max(dragCurent.y, fDragStart.y) };

                    fSelectionRect = { p0,p1};

                    ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &fSelectionRect, &CmdNull());
                
                }


            }

        if (IsLeftReleased == true)
        {
            isSelecting = false;
            fDragStart.x = -1;
            fDragStart.y = -1;
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
        
        if (isMouseUnderCursor  && evnt->window->IsMouseCursorInClientRect())
        {
            if (IsMiddlePressed)
                fAutoScroll.ToggleAutoScroll();

            if (IsLeftDoubleClick)
            {
                ToggleFullScreen();
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

    void TestApp::SetUserMessage(const std::string& message)
    {
        OIV_CMD_CreateText_Request request;
        OIV_CMD_CreateText_Response response;
        
        std::wstring wmsg = L"<textcolor=#ff8930>";
        wmsg += LLUtils::StringUtility::ToWString(message);
        
        request.text = const_cast<wchar_t*>( wmsg.c_str());
        request.backgroundColor = LLUtils::Color(0, 0, 0, 180);
        request.fontPath = L"C:\\Windows\\Fonts\\consola.ttf";
        //request.fontPath = L"C:\\Windows\\Fonts\\ahronbd.ttf";
        request.fontSize = 26;

        if (fUserMessageOverlayProperties.imageHandle != ImageHandleNull)
            OIVCommands::UnloadImage(fUserMessageOverlayProperties.imageHandle);
        

        if (ExecuteCommand(OIV_CMD_CreateText, &request, &response) == RC_Success)
        {
            fUserMessageOverlayProperties.imageHandle = response.imageHandle;
            fUserMessageOverlayProperties.opacity = 1.0;
            if (ExecuteCommand(OIV_CMD_ImageProperties, &fUserMessageOverlayProperties, &CmdNull()) == RC_Success)
            {
                fRefreshOperation.Queue();
                SetTimer(fWindow.GetHandle(), cTimerIDHideUserMessage, fDelayRemoveMessage, nullptr);
            }
        }
    }

    void TestApp::HideUserMessage()
    {
        if (fUserMessageOverlayProperties.opacity == 1.0)
        {
            // start exponential fadeout after after 'fDelayRemoveMessage' milliseconds.
            SetTimer(fWindow.GetHandle(), cTimerIDHideUserMessage, 5, nullptr);
            fUserMessageOverlayProperties.opacity = 0.99;
        }

        fUserMessageOverlayProperties.opacity = fUserMessageOverlayProperties.opacity * 0.8;// fUserMessageOverlayProperties.opacity;
        if (fUserMessageOverlayProperties.opacity < 0.01)
        {
            fUserMessageOverlayProperties.opacity = 0;
            KillTimer(fWindow.GetHandle(), cTimerIDHideUserMessage);
        }

        ExecuteCommand(OIV_CMD_ImageProperties, &fUserMessageOverlayProperties, &CmdNull());
        fRefreshOperation.Queue();
    }

    bool TestApp::ExecuteUserCommand(const CommandManager::CommandRequest& request)
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
}
