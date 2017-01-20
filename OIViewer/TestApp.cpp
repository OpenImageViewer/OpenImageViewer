#include "TestApp.h"
#include "FileMapping.h"
#include "StringUtility.h"
#include <limits>
#include <iomanip>
#include "win32/Win32Window.h"
#include <windowsx.h>
#include <tchar.h>
#include "win32/MonitorInfo.h"
#include <sstream>
#include <API\functions.h>
#include "win32/Win32Helper.h"
#include <filesystem>
#include <thread>
#include "FileHelper.h"
#include <cassert>
#include "StopWatch.h"
#include <PlatformUtility.h>

namespace OIV
{
    template <class T,class U>
    bool TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        ResultCode result = (ResultCode)OIV_Execute(command, sizeof(T), request, sizeof(U), response);

        if (result != ResultCode::RC_Success && result != ResultCode::RC_FileNotSupported)
            throw std::runtime_error("Could not execute command");

        
        return result == ResultCode::RC_Success;
            
    }

    TestApp::TestApp() :
          fKeyboardPanSpeed(100)
        , fKeyboardZoomSpeed(0.1)
        , fIsSlideShowActive(false)
        , fFilterlevel(0)
        , fIsGridEnabled(false)
        , fCurrentFileIndex(std::numeric_limits<LLUtils::ListString::size_type>::max())
    {
        new MonitorInfo();
    }

    TestApp::~TestApp()
    {
    }

    HWND TestApp::GetWindowHandle() const
    {
        return fWindow.GetHandle();
    }

    void TestApp::DisplayImage(ImageHandle image_handle)
    {
        OIV_CMD_DisplayImage_Request displayRequest = {};

        displayRequest.handle = image_handle;
        displayRequest.displayFlags = OIV_CMD_DisplayImage_Flags::DF_ResetScrollState;
        bool success = ExecuteCommand(CommandExecute::OIV_CMD_DisplayImage, &displayRequest, &CmdNull()) == true;
    }

    void TestApp::UpdateFileInfo(const OIV_CMD_LoadFile_Response& loadResponse, const long double& totalLoadTime)
    {
        std::wstringstream ss;
        
        ss << loadResponse.width << L" X " << loadResponse.height << L" X "
            << loadResponse.bpp << L" BPP | loaded in " << std::fixed << std::setprecision(1) << loadResponse.loadTime <<
            L"/" << totalLoadTime << L" ms";
                
            
        fWindow.SetStatusBarText(ss.str(), 0, 0);
        fLastOpenedFileHandle = loadResponse.handle;
    }

    bool TestApp::LoadFile(std::wstring filePath, bool onlyRegisteredExtension)
    {
        using namespace LLUtils;
        FileMapping fileMapping(filePath);
        void* buffer = fileMapping.GetBuffer();
        std::size_t size = fileMapping.GetSize();
        std::string extension = StringUtility::ToAString(StringUtility::GetFileExtension(filePath));


        if (buffer != nullptr && LoadFile((uint8_t*)buffer, size, extension, onlyRegisteredExtension) == true)
        {
                /*QryFileInformation fileInfo;
                ExecuteCommand(CE_GetFileInformation, &CmdNull(), &fileInfo);*/
                fOpenedFile = filePath;
                std::wstringstream ss;

                ss << filePath << L" - OpenImageViewer";
                HWND handle = GetWindowHandle();
                SetWindowTextW(handle, ss.str().c_str());
                UpdateFileInddex();
                return true;
        }
        
        return false;
    }

    bool TestApp::LoadFile(const uint8_t* buffer,  const std::size_t size,std::string extension, bool onlyRegisteredExtension)
    {
        using namespace LLUtils;
        OIV_CMD_LoadFile_Response loadResponse;
        OIV_CMD_LoadFile_Request loadRequest = {};

        loadRequest.buffer = (void*)buffer;
        loadRequest.length = size;
        std::string fileExtension = extension; 
        strcpy_s(loadRequest.extension, OIV_CMD_LoadFile_Request::EXTENSION_SIZE, fileExtension.c_str());
        loadRequest.flags = static_cast<OIV_CMD_LoadFile_Flags>(loadRequest.flags | (onlyRegisteredExtension ? OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension : 0));
        StopWatch stopWatch(true);
        bool success  = ExecuteCommand(CommandExecute::OIV_CMD_LoadFile, &loadRequest, &loadResponse) == true;
        stopWatch.Stop();
        if (success)
        {
            UpdateFileInfo(loadResponse,stopWatch.GetElapsedTimeReal(StopWatch::TimeUnit::Milliseconds));
            DisplayImage(fLastOpenedFileHandle);
        }

        return success;
    }

    void TestApp::LoadFileInFolder(std::wstring filePath)
    {
        fListFiles.clear();
        fCurrentFileIndex = std::numeric_limits<LLUtils::ListString::size_type>::max();
        
        std::experimental::filesystem::path workingPath  = filePath;
        std::experimental::filesystem::path fullFilePath = filePath;

        workingPath = workingPath.parent_path();
        
        if (workingPath.empty() == true)
        {
            TCHAR path[MAX_PATH];
            if (GetCurrentDirectory(MAX_PATH, reinterpret_cast<LPTSTR>(&path)) != 0)
            {
                workingPath = path;
                fullFilePath = workingPath / filePath;
            }
        }

        if (workingPath.empty() == false)
        {
            LLUtils::PlatformUtility::find_files(workingPath.wstring(), fListFiles);
            LLUtils::ListStringIterator it = std::find(fListFiles.begin(), fListFiles.end(), fullFilePath.wstring());
            if (it != fListFiles.end())
                fCurrentFileIndex = std::distance(fListFiles.begin(), it);

            UpdateFileInddex();
        }
    }

    void TestApp::OnScroll(LLUtils::PointI32 panAmount)
    {
        Pan(panAmount.x, panAmount.y);
    }

    void TestApp::Run(std::wstring filePath)
    {
        using namespace std;
        using namespace std::placeholders;

        std::size_t bufferSize = 0;
        unique_ptr<uint8_t> buffer;
        
        
        const bool isInitialFile = filePath.empty() == false && experimental::filesystem::exists(filePath);
        
        std::thread t;
        string extension;
        // Load file into memory in a secondary thread.
        if (isInitialFile == true)
        {
            t = std::thread
            (
                [&filePath, &buffer, &bufferSize]()
            {
                uint8_t* tmp = nullptr;
                LLUtils::File::ReadAllBytes(filePath, bufferSize, tmp);
                buffer = unique_ptr<uint8_t>(tmp);
            });

            // Extract the file extension
            std::experimental::filesystem::path p = filePath;
            extension = p.extension().string();
            if (extension.empty() == false)
                extension = extension.substr(1, extension.length() - 1);
        }

        
        // initialize the windowing system of the window
        HINSTANCE moduleHanle = GetModuleHandle(nullptr);
        fWindow.Create(moduleHanle, SW_SHOW);
        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this,_1));
        

        // Init OIV
        CmdDataInit init;
        init.parentHandle = reinterpret_cast<std::size_t>(fWindow.GetHandleClient());
        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());
        UpdateWindowSize(nullptr);
        

        if (isInitialFile == true)
        {
            // wait for file to finish loading and 
            t.join();
            if (LoadFile(buffer.get(), bufferSize, extension, false) == true)
                fOpenedFile = filePath;
        }

        //Set linear image filtering
        SetFilterLevel(FT_Linear);

        // Load all files in the directory of the loaded file
        LoadFileInFolder(filePath);
        
        Win32Helper::MessageLoop();

        // Destry OIV when windows is closed.
        ExecuteCommand(OIV_CMD_Destroy, &CmdNull(), &CmdNull());
    }



    void TestApp::UpdateFileInddex()
    {
        if (fListFiles.empty())
            return;

        std::wstringstream ss;
        ss << L"File " << (fCurrentFileIndex == std::numeric_limits<LLUtils::ListString::size_type>::max() ? 
            0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
    }

    void TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return;

        LLUtils::ListString::size_type totalFiles = fListFiles.size();
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
            fCurrentFileIndex = static_cast<LLUtils::ListString::size_type>(fileIndex);
        }


        UpdateFileInddex();

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

    void TestApp::SetFilterLevel(int filterLevel)
    {
        
        OIV_CMD_Filter_Request filter;

        filter.filterType = static_cast<OIV_Filter_type>(filterLevel);
        if (ExecuteCommand(CE_FilterLevel, &filter, &CmdNull()))
            fFilterlevel = filterLevel;
    }

    void TestApp::ToggleGrid()
    {
        CmdRequestTexelGrid grid;
        fIsGridEnabled = !fIsGridEnabled;
        grid.gridSize = fIsGridEnabled ? 1.0 : 0.0;
        if (ExecuteCommand(CE_TexelGrid, &grid, &CmdNull()))
        {
            
        }
        
    }

    void TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
        bool IsAlt = (GetKeyState(VK_MENU) & static_cast<USHORT>(0x8000)) != 0;
        bool IsControl = (GetKeyState(VK_CONTROL) & static_cast<USHORT>(0x8000)) != 0;
        bool IsShift = (GetKeyState(VK_SHIFT) & static_cast<USHORT>(0x8000)) != 0;

        switch (evnt->message.wParam)
        {
        case 'V':
            TransformImage(AAT_FlipVertical);
        case 'H':
            TransformImage(AAT_FlipHorizontal);
                break;
        case VK_OEM_4: // '['
            TransformImage(AAT_Rotate90CCW);
            break;
        case VK_OEM_6: // ']'
            TransformImage(AAT_Rotate90CW);
            
            break;
        case 'Q':
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case 'F':
            ToggleFullScreen();
            break;
        case VK_DOWN:
        case VK_RIGHT:
        case VK_NEXT:
            JumpFiles(1);
            break;
        case VK_UP:
        case VK_LEFT:
        case VK_PRIOR:
            JumpFiles(-1);
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
            SetFilterLevel(fFilterlevel + 1);
            break;
        case VK_OEM_COMMA:
            SetFilterLevel(fFilterlevel - 1);
            break;
        case VK_NUMPAD8:
            Pan(0, -fKeyboardPanSpeed);
            break;
        case VK_NUMPAD2:
            Pan(0, fKeyboardPanSpeed);
            break;
        case VK_NUMPAD4:
            Pan(-fKeyboardPanSpeed, 0);
            break;
        case VK_NUMPAD6:
            Pan(fKeyboardPanSpeed, 0);
            break;
        case VK_ADD:
            Zoom(fKeyboardZoomSpeed, -1, -1);
            break;
        case VK_SUBTRACT:
            Zoom(-fKeyboardZoomSpeed, -1, -1);
            break;
        case VK_NUMPAD5:
            // TODO: center
            break;
        case VK_MULTIPLY:
            //TODO: center and reset zoom
            break;
        case VK_DIVIDE:
            break;
        case 'G':
            ToggleGrid();
            break;
        case 'P':
        {
            std::wstring command = LR"(c:\Program Files\Adobe\Adobe Photoshop CC 2017\Photoshop.exe)";
            ShellExecute(nullptr, L"open", command.c_str(), fOpenedFile.c_str(), nullptr, SW_SHOWDEFAULT);
        }
        break;

        }
    }

    void TestApp::Pan(int horizontalPIxels, int verticalPixels )
    {
        CmdDataPan pan;
        pan.x = horizontalPIxels;
        pan.y = verticalPixels;
        ExecuteCommand(CommandExecute::CE_Pan, &pan, &(CmdNull()));
    }

    void TestApp::Zoom(double precentage, int zoomX , int zoomY )
    {
        CmdDataZoom zoom{ precentage,zoomX, zoomY };
        ExecuteCommand(CommandExecute::CE_Zoom, &zoom, &CmdNull());
        UpdateCanvasSize();
    }

    void TestApp::UpdateCanvasSize()
    {
        CmdGetNumTexelsInCanvasResponse response;
        
        if (ExecuteCommand(CMD_GetNumTexelsInCanvas, &CmdNull(), &response))
        {

            std::wstringstream ss;
            ss << _T("Canvas: ")
                << std::fixed << std::setprecision(1) << std::setfill(_T(' ')) << std::setw(6) << response.width
                << _T(" X ")
                << std::fixed << std::setprecision(1) << std::setfill(_T(' ')) << std::setw(6) << response.height;
            fWindow.SetStatusBarText(ss.str(), 3, 0);
        }
    }

    void TestApp::UpdateTexelPos()
    {
        POINT p = fWindow.GetMousePosition();
        CmdRequestTexelAtMousePos request;
        CmdResponseTexelAtMousePos response;
        request.x = p.x;
        request.y = p.y;
        if (ExecuteCommand(CE_TexelAtMousePos, &request, &response))
        {
            std::wstringstream ss;
            ss << _T("Texel: ") 
               << std::fixed << std::setprecision(1) << std::setfill(_T(' ')) << std::setw(6) << response.x
               << _T(" X ") 
               << std::fixed << std::setprecision(1) << std::setfill(_T(' ')) << std::setw(6) << response.y;
            fWindow.SetStatusBarText(ss.str(), 2, 0);
        }
    }


    void TestApp::UpdateWindowSize(const Win32::EventWinMessage* winMessage)
    {
        if (winMessage == nullptr || winMessage->window->GetHandleClient() == winMessage->message.hwnd)
        {
            SIZE size = fWindow.GetClientSize();
            ExecuteCommand(CMD_SetClientSize,
                &CmdSetClientSizeRequest{ static_cast<uint16_t>(size.cx),
                static_cast<uint16_t>(size.cy) }, &CmdNull());
            UpdateCanvasSize();
        }
    }

    void TestApp::TransformImage(OIV_AxisAlignedRTransform transform)
    {
        ExecuteCommand(OIV_CMD_AxisAlignedTransform,
            &OIV_CMDAxisalignedTransformRequest{ transform }, &CmdNull());
    }

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
        const MSG& uMsg = evnt->message;
        switch (uMsg.message)
        {
        case WM_WINDOWPOSCHANGED:
            UpdateWindowSize(evnt);
            break;

        case WM_TIMER:
            if (uMsg.wParam == cTimerID)
                JumpFiles(1);
        break;

        case AutoScroll::PRIVATE_WN_AUTO_SCROLL:
            fAutoScroll.PerformAutoScroll(evnt);
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            handleKeyInput(evnt);
            break;

        case WM_MOUSEMOVE:
            UpdateTexelPos();
            break;
        break;
        }
        return true;
    }
    

    bool TestApp::HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file)
    {
        if (LoadFile(event_ddrag_drop_file->fileName, false))
        {
            LoadFileInFolder(event_ddrag_drop_file->fileName);
        }
        return true;
        
    }

    void TestApp::HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt)
    {
        using namespace Win32;
        
        const RawInputMouseWindow& mouseState = evnt->window->GetMouseState();

        const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsRightCatured = mouseState.IsCaptured(MouseState::Button::Right);
        const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        const bool IsRightPressed = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::State::Pressed;
        const bool IsLeftPressed = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::State::Pressed;
        const bool IsMiddlePressed = evnt->GetButtonEvent(MouseState::Button::Middle) == MouseState::State::Pressed;
        const bool isMouseInsideWindowAndfocus = evnt->window->IsMouseCursorInClientRect() && evnt->window->IsInFocus();
        if (IsRightCatured == true)
        {
            if (evnt->DeltaX != 0 || evnt->DeltaY != 0)
                Pan(-evnt->DeltaX, -evnt->DeltaY);
        }

        LONG wheelDelta = evnt->DeltaWheel;
        if (wheelDelta != 0)
        {
            if (IsRightCatured || isMouseInsideWindowAndfocus)
            {
                POINT mousePos = fWindow.GetMousePosition();
                //20% percent zoom in each wheel step
                if (IsRightCatured)
                    //  Zoom to center if currently panning.
                    Zoom(wheelDelta * 0.2);
                else
                    Zoom(wheelDelta * 0.2, mousePos.x, mousePos.y);
            }
        }
        
        if (IsMiddlePressed && isMouseInsideWindowAndfocus)
        {
            fAutoScroll.ToggleAutoScroll();
        }

        if (     isMouseInsideWindowAndfocus 
             && (IsRightPressed && IsLeftPressed)
             || (IsRightPressed && IsLeftDown)
             || (IsRightDown && IsLeftPressed))
        {
            ToggleFullScreen();
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
}
