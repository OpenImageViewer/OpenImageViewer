#define NOMINMAX
#include "TestApp.h"
#include "Utility.h"
#include "FileMapping.h"
#include "StringUtility.h"
#include <limits>
#include <iomanip>
#include "win32/Win32Window.h"
#include <windowsx.h>
#include <tchar.h>
#include "win32/MonitorInfo.h"
#include <sstream>
#include <iostream>
#include <API\functions.h>
#include "win32/Win32Helper.h"
#include <filesystem>
#include <thread>
#include "FileHelper.h"


namespace OIV
{
    template <class T,class U>
    bool TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return
            OIV_Execute(command, sizeof(T), request, sizeof(U), response) == ResultCode::RC_Success;
            
    }

    TestApp::TestApp() :
          fKeyboardPanSpeed(100)
        , fKeyboardZoomSpeed(0.1)
        , fIsSlideShowActive(false)
        , fFilterlevel(0)
        , fIsGridEnabled(false)
        , fCurrentFileIndex(-1)
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

    void TestApp::UpdateFileInfo(const CmdResponseLoad& loadResponse)
    {
        std::wstringstream ss;
        
        ss << loadResponse.width << L" X " << loadResponse.height << L" X " 
            << loadResponse.bpp << L" BPP | loaded in " << std::fixed << std::setprecision(3) << loadResponse.loadTime << " ms";
        fWindow.SetStatusBarText(ss.str(), 0, 0);
    }

    bool TestApp::LoadFile(std::wstring filePath, bool onlyRegisteredExtension)
    {
        FileMapping fileMapping(filePath);
        void* buffer = fileMapping.GetBuffer();
        size_t size = fileMapping.GetSize();
        std::string extension = StringUtility::ToAString(StringUtility::GetFileExtension(filePath));


        if (LoadFile((uint8_t*)buffer, size, extension, onlyRegisteredExtension) == true)
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

    bool TestApp::LoadFile(const uint8_t* buffer,  const size_t size,std::string extension, bool onlyRegisteredExtension)
    {
        CmdResponseLoad loadResponse;
        CmdDataLoadFile loadRequest;

        loadRequest.buffer = (void*)buffer;
        loadRequest.length = size;
        std::string fileExtension = extension; 
        strcpy_s(loadRequest.extension, CmdDataLoadFile::EXTENSION_SIZE, fileExtension.c_str());
        loadRequest.onlyRegisteredExtension = onlyRegisteredExtension;

        bool success  = ExecuteCommand(CommandExecute::CE_LoadFile, &loadRequest, &loadResponse) == true;
        if (success)
        {
            UpdateFileInfo(loadResponse);
        }

        return success;
    }

    void TestApp::LoadFileInFolder(std::wstring filePath)
    {
        fListFiles.clear();
        fCurrentFileIndex = -1;

        std::wstring workingFolder;
        int idx = filePath.find_last_of(L"\\");

        if (idx > -1)
        {
            workingFolder = filePath.substr(0, idx);
        }
        else
        {
            TCHAR path[MAX_PATH];
            if (GetCurrentDirectory(MAX_PATH, reinterpret_cast<LPTSTR>(&path)) != 0)
            {
                workingFolder = path;
                if (workingFolder.at(workingFolder.length() - 1) == '\\')
                    workingFolder.erase(workingFolder.length() - 1, 1);

                filePath = workingFolder + L"\\" + filePath;
            }

        }

        if (workingFolder.empty() == false)
        {
            Utility::find_files(workingFolder, fListFiles);
            ListFilesIterator it = std::find(fListFiles.begin(), fListFiles.end(), filePath);
            if (it != fListFiles.end())
                fCurrentFileIndex = std::distance(fListFiles.begin(), it);

            UpdateFileInddex();
        }
    }
    void TestApp::Run(std::wstring filePath)
    {
        using namespace std;
        using namespace std::placeholders;

        size_t bufferSize = 0;
        uint8_t* buffer = nullptr;
        
        
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
                OIV::File::ReadAllBytes(filePath, bufferSize, buffer);
            }
            );

            // Extract the file extension
            std::experimental::filesystem::path p = filePath;
            extension = p.extension().string();
            if (extension.empty() == false)
                extension = extension.substr(1, extension.length() - 1);
        }

        
        // initialize the windowing system of the window
        HINSTANCE moduleHanle = GetModuleHandle(NULL);
        fWindow.Create(moduleHanle, SW_SHOW);
        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this,_1));
        

        //Init OIV
        CmdDataInit init;
        init.parentHandle = reinterpret_cast<size_t>(fWindow.GetHandleClient());
        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());
        UpdateWindowSize(NULL);
        

        if (isInitialFile == true)
        {
            // wait for file to finish loading and 
            t.join();
            LoadFile(buffer, bufferSize, extension, false);
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
        ss << L"File " << (fCurrentFileIndex == -1 ? 0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
    }

    void TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return;

        int totalFiles = fListFiles.size();

        int fileIndex = fCurrentFileIndex ;


        int sign;
        if (step == std::numeric_limits<int>::max())
        {
            // Last
            fileIndex = fListFiles.size();
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
        ListFilesIterator it;

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
            fCurrentFileIndex = fileIndex;


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
            SetTimer(hwnd, cTimerID, 3000, NULL);
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
        bool IsAlt = (GetKeyState(VK_MENU) & (USHORT)0x8000) != 0;
        bool IsControl = (GetKeyState(VK_CONTROL) & (USHORT)0x8000) != 0;
        bool IsShift = (GetKeyState(VK_SHIFT) & (USHORT)0x8000) != 0;

        switch (evnt->message.wParam)
        {
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
            ShellExecute(NULL, L"open", command.c_str(),fOpenedFile.c_str(), NULL, SW_SHOWDEFAULT);
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
        if (winMessage == NULL || winMessage->window->GetHandleClient() == winMessage->message.hwnd)
        {
            SIZE size = fWindow.GetClientSize();
            ExecuteCommand(CMD_SetClientSize,
                &CmdSetClientSizeRequest{ static_cast<uint16_t>(size.cx),
                static_cast<uint16_t>(size.cy) }, &CmdNull());
            UpdateCanvasSize();
        }
    }

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
        const MSG& uMsg = evnt->message;
        static int lastX = -1;
        static int lastY = -1;
        switch (uMsg.message)
        {
        case WM_WINDOWPOSCHANGED:
            UpdateWindowSize(evnt);
            break;

        case WM_TIMER:
        {
            if (uMsg.wParam == cTimerID)
                JumpFiles(1);
        }
        break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            handleKeyInput(evnt);
            break;

        case WM_MOUSEWHEEL:
        {
            int zDelta = (GET_WHEEL_DELTA_WPARAM(uMsg.wParam) / WHEEL_DELTA);
            //20% percent zoom in each wheel step
            POINT mousePos = fWindow.GetMousePosition();
            Zoom(zDelta * 0.2, mousePos.x, mousePos.y);
        }
        break;

        case WM_MBUTTONDOWN:
            ToggleFullScreen();
            break;

        case WM_LBUTTONDOWN:
            SetCapture(uMsg.hwnd);
            break;

        case WM_LBUTTONUP:
            ReleaseCapture();
            lastX = -1;
            break;

        case WM_MOUSEMOVE:
        {
            UpdateTexelPos();
            if (GetCapture() == nullptr)
                return false;
            int xPos = GET_X_LPARAM(uMsg.lParam);
            int yPos = GET_Y_LPARAM(uMsg.lParam);

            int deltax = xPos - lastX;
            int deltaY = yPos - lastY;

            bool isLeftDown = uMsg.wParam & MK_LBUTTON;

            if (isLeftDown)
            {
                if (lastX != -1)
                {
                    Pan(-deltax, -deltaY);
                }

                lastX = xPos;
                lastY = yPos;
            }
        }
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

    bool TestApp::HandleMessages(const Win32::Event* evnt1)
    {

        const Win32::EventWinMessage* evnt = dynamic_cast<const Win32::EventWinMessage*>(evnt1);

        if (evnt != nullptr)
            return HandleWinMessageEvent(evnt);

        const Win32::EventDdragDropFile* dragDropEvent = dynamic_cast<const Win32::EventDdragDropFile*>(evnt1);

        if (dragDropEvent != nullptr)
            return HandleFileDragDropEvent(dragDropEvent);

        return false;

    }
}
