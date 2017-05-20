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
#include <tchar.h>
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

namespace OIV
{
    template <class T,class U>
    ResultCode TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return OIV_Execute(command, sizeof(T), request, sizeof(U), response);
    }

    TestApp::TestApp() 
    {
        
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
        displayRequest.displayFlags = static_cast<OIV_CMD_DisplayImage_Flags>(OIV_CMD_DisplayImage_Flags::DF_ResetScrollState | OIV_CMD_DisplayImage_Flags::DF_ApplyExifTransformation);
        
        ExecuteCommand(CommandExecute::OIV_CMD_DisplayImage, &displayRequest, &CmdNull());
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

    void TestApp::UpdateZoomScrollState()
    {
        OIV_CMD_ZoomScrollState_Request request;
        const Serialization::UserSettingsData& settings = fSettings.getUserSettings();
        request.innerMarginsX = settings.zoomScrollState.InnnerMargins.x;
        request.innerMarginsY = settings.zoomScrollState.InnnerMargins.y;
        request.outermarginsX = settings.zoomScrollState.OuterMargins.x;
        request.outermarginsY = settings.zoomScrollState.OuterMargins.y;
        request.SmallImageOffsetStyle = settings.zoomScrollState.smallImageOffsetStyle;
        
        ExecuteCommand(OIV_CMD_ZoomScrollState, &request, &CmdNull());
    }

  
    void TestApp::FinalizeImageLoad()
    {
        // Enter this function only from the main thread.
        assert("TestApp::FinalizeImageLoad() can be called only from the main thread" &&
            GetCurrentThreadId() == fMainThreadID);
        
        LLUtils::StopWatch stopWatch(true);
        DisplayImage(fImageBeingOpened.imageHandle);
        fImageBeingOpened.displayTime = stopWatch.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);


        fOpenedImage = fImageBeingOpened;
        fImageBeingOpened = ImageDescriptor();

        UpdateTitle();
        UpdateStatusBar();
        UpdateFileInddex();
    }

    void TestApp::NotifyImageLoaded()
    {
        if (GetCurrentThreadId() == fMainThreadID)
        {
            FinalizeImageLoad();
        }
        else
        {
            // Wait for the main window to get initialized.
            std::unique_lock<std::mutex> ul(fMutexWindowCreation);
            
            // send message to main thread.
            PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_NOTIFY_LOADED, 0, 0);
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

    void TestApp::UnloadFile()
    {
        OIV_CMD_UnloadFile_Request unloadRequest = {};
        if (fOpenedImage.imageHandle != ImageNullHandle)
        {
            unloadRequest.handle = fOpenedImage.imageHandle;
            ExecuteCommand(CommandExecute::OIV_CMD_UnloadFile, &unloadRequest, &CmdNull());
            fOpenedImage.imageHandle = ImageNullHandle;
        }
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
            UnloadFile();
            fImageBeingOpened.width = loadResponse.width;
            fImageBeingOpened.height = loadResponse.height;
            fImageBeingOpened.loadTime = loadResponse.loadTime;
            fImageBeingOpened.imageHandle = loadResponse.handle;
            fImageBeingOpened.bpp = loadResponse.bpp;
            NotifyImageLoaded();
        }

        return result == RC_Success;
    }
    
    

    void TestApp::LoadFileInFolder(std::wstring absoluteFilePath)
    {
        using namespace std::experimental::filesystem;
        fListFiles.clear();
        fCurrentFileIndex = std::numeric_limits<LLUtils::ListString::size_type>::max();
        
        std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();

        LLUtils::PlatformUtility::find_files(absoluteFolderPath, fListFiles);
        LLUtils::ListStringIterator it = std::find(fListFiles.begin(), fListFiles.end(), absoluteFilePath);
        if (it != fListFiles.end())
            fCurrentFileIndex = std::distance(fListFiles.begin(), it);

        UpdateFileInddex();
    }

    void TestApp::OnScroll(LLUtils::PointI32 panAmount)
    {
        Pan(panAmount.x, panAmount.y);
    }


    void TestApp::Run(std::wstring relativeFilePath)
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
        fWindow.Create(GetModuleHandle(nullptr), SW_SHOW);
        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this, _1));


        if (isInitialFile == true)
            fMutexWindowCreation.unlock();
        
        
        // Init OIV renderer
        CmdDataInit init;
        init.parentHandle = reinterpret_cast<std::size_t>(fWindow.GetHandleClient());
        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());

        // Update client size
        UpdateWindowSize(nullptr);

        // wait for initial file to finish loading
        if (asyncResult.valid())
            asyncResult.wait();
        
        // load settings
        fSettings.Load();


        UpdateZoomScrollState();

        // Load all files in the directory of the loaded file
        if (filePath.empty() == false)
            LoadFileInFolder(filePath);
        
        Win32Helper::MessageLoop();

        // Destroy OIV when window is closed.
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

    void TestApp::SetFilterLevel(OIV_Filter_type filterType)
    {
        
        OIV_CMD_Filter_Request filter;

        filter.filterType = static_cast<OIV_Filter_type>( std::min(OIV_Filter_type::FT_Count - 1,
            std::max(static_cast<int>(OIV_Filter_type::FT_None), static_cast<int>(filterType)) ));
        if (ExecuteCommand(CE_FilterLevel, &filter, &CmdNull()) == RC_Success)
            fFilterType = filter.filterType;
    }

    void TestApp::ToggleGrid()
    {
        CmdRequestTexelGrid grid;
        fIsGridEnabled = !fIsGridEnabled;
        grid.gridSize = fIsGridEnabled ? 1.0 : 0.0;
        if (ExecuteCommand(CE_TexelGrid, &grid, &CmdNull()) == RC_Success)
        {
            
        }
        
    }

    void TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
        bool IsAlt =  (GetKeyState(VK_MENU) & static_cast<USHORT>(0x8000)) != 0;
        bool IsControl = (GetKeyState(VK_CONTROL) & static_cast<USHORT>(0x8000)) != 0;
        bool IsShift = (GetKeyState(VK_SHIFT) & static_cast<USHORT>(0x8000)) != 0;

        switch (evnt->message.wParam)
        {
        case 'C':
            if (IsControl)
                CopyVisibleToClipBoard();
            break;
        case 'V':
            if (IsControl == true)
                PasteFromClipBoard();
            else
                TransformImage(AAT_FlipVertical);
            break;
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
            SetFilterLevel(static_cast<OIV_Filter_type>( static_cast<int>(fFilterType) + 1));
            break;
        case VK_OEM_COMMA:
            SetFilterLevel(static_cast<OIV_Filter_type>(static_cast<int>(fFilterType) - 1));
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
            ShellExecute(nullptr, L"open", command.c_str(),  fOpenedImage.fileName.c_str(), nullptr, SW_SHOWDEFAULT);
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
        
        if (ExecuteCommand(CMD_GetNumTexelsInCanvas, &CmdNull(), &response) == RC_Success)
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
        if (ExecuteCommand(CE_TexelAtMousePos, &request, &response) == RC_Success)
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
        OIVCommands::TransformImage(ImageHandleDisplayed, transform);
    }

    void TestApp::LoadRaw(const uint8_t* buffer, uint32_t width, uint32_t height, OIV_TexelFormat texelFormat)
    {
        using namespace LLUtils;
        
        OIV_CMD_LoadRaw_Response loadResponse;
        OIV_CMD_LoadRaw_Request loadRequest = {};
        loadRequest.buffer = const_cast<uint8_t*>(buffer);
        loadRequest.width = width;
        loadRequest.height = height;
        loadRequest.texelFormat = texelFormat;
        loadRequest.transformation = OIV_AxisAlignedRTransform::AAT_FlipVertical;
        
        
        ResultCode result = ExecuteCommand(CommandExecute::OIV_CMD_LoadRaw, &loadRequest, &loadResponse);
        if (result == RC_Success)
        {
            UnloadFile();
            fImageBeingOpened = ImageDescriptor();
            fImageBeingOpened.width = loadRequest.width;
            fImageBeingOpened.height = loadRequest.height;
            fImageBeingOpened.loadTime = loadResponse.loadTime;
            fImageBeingOpened.source = ImageSource::IS_Clipboard;
            OIV_Util_GetBPPFromTexelFormat(texelFormat, &fImageBeingOpened.bpp);
            fImageBeingOpened.imageHandle = loadResponse.handle;
            NotifyImageLoaded();
        }

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
        OIV_CMD_WindowToImage_Request request;
        OIV_CMD_WindowToImage_Response response;

        request.rect = fSelectionRect;
        ResultCode result = ExecuteCommand(OIV_CMD_WindowToimage, &request, &response);

        if (result == RC_Success)
        {
            OIV_CMD_CropImage_Request requestCropImage;
            OIV_CMD_CropImage_Response responseCropImage;

            requestCropImage.rect = { (int)response.rect.x0 ,(int)response.rect.y0, (int)response.rect.x1, (int)response.rect.y1 };
            requestCropImage.imageHandle = ImageHandleDisplayed;

            // 1. create a new cropped image 
            ResultCode imageCreated = ExecuteCommand(OIV_CMD_CropImage, &requestCropImage, &responseCropImage);

            if (result == RC_Success)
            {

                struct unloadImage
                {
                    ImageHandle handle;
                    ~unloadImage()
                    {
                        if (handle != ImageNullHandle)
                            OIVCommands::UnloadImage(handle) != RC_Success;
                                
                    }
                } handle{ responseCropImage.imageHandle };


                //2. Flip the image vertically and convert it to BGRA for the clipboard.
                result = OIVCommands::TransformImage(responseCropImage.imageHandle, OIV_AxisAlignedRTransform::AAT_FlipVertical);
                result = OIVCommands::ConvertImage(responseCropImage.imageHandle, OIV_TexelFormat::TF_I_B8_G8_R8_A8);

                //3

                OIV_CMD_GetPixels_Request requestGetPixels;
                OIV_CMD_GetPixels_Response responseGetPixels;

                requestGetPixels.handle = responseCropImage.imageHandle;

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

                    
                   

                    uint32_t width = responseGetPixels.width;// responseCropImage.width;
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
        case Win32::UserMessage::PRIVATE_WN_NOTIFY_LOADED:
            NotifyImageLoaded();
        break;
        
        case Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL:
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
        const bool IsLeftCatured = mouseState.IsCaptured(MouseState::Button::Left);
        const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        const bool IsRightPressed = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::State::Pressed;
        const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::State::Released;
        const bool IsLeftPressed = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::State::Pressed;
        const bool IsMiddlePressed = evnt->GetButtonEvent(MouseState::Button::Middle) == MouseState::State::Pressed;
        const bool isMouseInsideWindowAndfocus = evnt->window->IsMouseCursorInClientRect() && evnt->window->IsInFocus();
        
        static bool isSelecting = false;

        if (IsLeftPressed == true)
        {
            if (fDragStart.x == -1)
            {
                fSelectionRect = { -1,-1,-1,-1};

                ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &fSelectionRect, &CmdNull()) == true;
                //Disable selection rect
            }
            fDragStart = evnt->window->GetMousePosition();
            
        }

        else
            if (IsLeftCatured == true)
            {
                LLUtils::PointI32 dragCurent = evnt->window->GetMousePosition();
                if (isSelecting == true || dragCurent.DistanceSquared(fDragStart) > 225)
                {
                    isSelecting = true;
                    LLUtils::PointI32 p0 = { std::min(dragCurent.x, fDragStart.x), std::min(dragCurent.y, fDragStart.y) };
                    LLUtils::PointI32 p1 = { std::max(dragCurent.x, fDragStart.x), std::max(dragCurent.y, fDragStart.y) };

                    fSelectionRect = { p0.x,p0.y,p1.x,p1.y };

                    ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &fSelectionRect, &CmdNull()) == true;
                
                }


            }

        if (IsLeftReleased == true)
        {
            isSelecting = false;
            fDragStart.x = -1;
            fDragStart.y = -1;
        }

       
        

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
