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
#include <Rect.h>

namespace OIV
{
    template <class T,class U>
    ResultCode TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return OIV_Execute(command, sizeof(T), request, sizeof(U), response);
    }

    TestApp::TestApp()
        :fRefreshOperation(std::bind(&TestApp::OnRefresh,this))
    {
        
    }


    TestApp::~TestApp()
    {
        
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
        if (descriptor.imageHandle != ImageNullHandle)
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

        memcpy(buffer.get(), fOpenedImage.fileName.c_str(), stringLength * sizeof(wchar_t));


        buffer.get()[stringLength] = '\0';
        buffer.get()[stringLength + 1] = '\0';


        SHFILEOPSTRUCT file_op =
        {
            GetWindowHandle()                          // HWND            hwnd;
            , FO_DELETE                                 // UINT            wFunc;
            , buffer.get()                                  // PCZZWSTR        pFrom;
            , nullptr                                   // PCZZWSTR        pTo;
            , permanently ? 0 : FOF_ALLOWUNDO //FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT //FILEOP_FLAGS    fFlags;
            , FALSE                                     // BOOL            fAnyOperationsAborted;
            , nullptr                                   // LPVOID          hNameMappings;
            , nullptr                                   // PCWSTR          lpszProgressTitle; // only used if FOF_SIMPLEPROGRESS
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
            FitToClientArea();
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
            
            // make 'Load file in folder' the last operation after the initial file has loaded.
            LoadFileInFolder(fOpenedImage.fileName);
            
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
        fCurrentFileIndex = std::numeric_limits<LLUtils::ListString::size_type>::max();

        std::wstring absoluteFolderPath = path(absoluteFilePath).parent_path();

        LLUtils::PlatformUtility::find_files(absoluteFolderPath, fListFiles);

        UpdateOpenedFileIndex();
        UpdateUIFileIndex();
    }

    void TestApp::OnScroll(LLUtils::PointI32 panAmount)
    {
        Pan(panAmount.x, panAmount.y);
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
    }

    void TestApp::Destroy()
    {
        // Destroy OIV when window is closed.
        ExecuteCommand(OIV_CMD_Destroy, &CmdNull(), &CmdNull());
    }

    void TestApp::Run()
    {
        Win32Helper::MessageLoop();
    }

    void TestApp::UpdateUIFileIndex()
    {
        std::wstringstream ss;
        ss << L"File " << (fCurrentFileIndex == std::numeric_limits<LLUtils::ListString::size_type>::max() ? 
            0 : fCurrentFileIndex + 1) << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
    }

    bool TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return false;

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
        
        OIV_CMD_Filter_Request filter;

        filter.filterType = static_cast<OIV_Filter_type>( std::min(OIV_Filter_type::FT_Count - 1,
            std::max(static_cast<int>(OIV_Filter_type::FT_None), static_cast<int>(filterType)) ));
        if (ExecuteCommand(CE_FilterLevel, &filter, &CmdNull()) == RC_Success)
        {
            fFilterType = filter.filterType;
            fRefreshOperation.Queue();
        }
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

    void TestApp::handleKeyInput(const Win32::EventWinMessage* evnt)
    {
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
                CopyVisibleToClipBoard();
            else
                CropVisibleImage();
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
            Pan(0, fKeyboardPanSpeed);
            break;
        case VK_NUMPAD2:
            Pan(0, -fKeyboardPanSpeed);
            break;
        case VK_NUMPAD4:
            Pan(fKeyboardPanSpeed, 0);
            break;
        case VK_NUMPAD6:
            Pan(-fKeyboardPanSpeed, 0);
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
            FitToClientArea();
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
        break;

        }
    }

    void TestApp::SetOffset(LLUtils::PointI32 offset)
    {
        fOffset = ResolveOffset(offset);
        OIVCommands::SetOffset(fOffset);
        fRefreshOperation.Queue();
    }

    void TestApp::SetOriginalSize()
    {
        SetZoom(1.0, -1, -1);
        Center();
    }

    void TestApp::Pan(int horizontalPIxels, int verticalPixels )
    {
        using namespace LLUtils;
        SetOffset(PointI32(horizontalPIxels, verticalPixels) + fOffset);


    }

    void TestApp::Zoom(double amount, int zoomX , int zoomY )
    {
        if (amount > 0)
            amount = fZoom * (1 + amount);
        else
            if (amount < 0)
                amount = fZoom  / ( 1 - amount);
        
        SetZoom(amount, zoomX, zoomY);
      
    }
    
    void TestApp::FitToClientArea()
    {
        using namespace LLUtils;
        SIZE clientSize = fWindow.GetClientSize();
        PointF64 ratio = PointF64(clientSize.cx, clientSize.cy) / PointF64(fOpenedImage.width, fOpenedImage.height);
        double zoom = std::min(ratio.x, ratio.y);
        fRefreshOperation.Begin();
        SetZoom(zoom, -1, -1);
        Center();
        fRefreshOperation.End();
    }
    LLUtils::PointF64 TestApp::GetImageSize(bool visibleSize)
    {
        LLUtils::PointF64 imageSize = LLUtils::PointF64(fOpenedImage.width , fOpenedImage.height);
        if (visibleSize)
            imageSize *= fZoom;
        return imageSize;
    }

    void TestApp::UpdateUIZoom()
    {
        std::wstringstream ss;
        ss << L"Scale: " << std::fixed << std::setprecision(1);
        if (fZoom >= 1)
            ss << "x" << fZoom;
        else
            ss << "1/" << 1 / fZoom;

        fWindow.SetStatusBarText(ss.str(), 4, 0);
    }

    void TestApp::SetZoom(double amount, int x, int y)
    {
        using namespace LLUtils;

        //////
        PointF64 zoomPoint;
        if (x < 0 || y < 0)
            zoomPoint = GetImageSize(false) / 2;
        else
            zoomPoint = (PointF64(x, y) - static_cast<PointF64>(fOffset)) / fZoom;


        PointI32 offset = static_cast<PointI32>((zoomPoint / GetImageSize(false)) * (fZoom - amount) * GetImageSize(false));

        fZoom = amount;
        ///////

        fRefreshOperation.Begin();
        OIVCommands::SetZoom(fZoom);
        SetOffset(fOffset + offset);
        
        fRefreshOperation.End();

        UpdateCanvasSize();
        UpdateUIZoom();
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


    void TestApp::UpdateWindowSize()
    {
            SIZE size = fWindow.GetClientSize();
            ExecuteCommand(CMD_SetClientSize,
                &CmdSetClientSizeRequest{ static_cast<uint16_t>(size.cx),
                static_cast<uint16_t>(size.cy) }, &CmdNull());
            UpdateCanvasSize();
    }

    void TestApp::Center()
    {
        using namespace LLUtils;
        PointI32 offset = static_cast<PointI32>(PointF64(fWindow.GetClientSize()) - (PointF64(fOpenedImage.width, fOpenedImage.height) * fZoom)) / 2;
        SetOffset(offset);
    }

    int CalculateOffset(int clientSize, int imageSize, int offset, double margin)
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
        return static_cast<int>(fixedOffset);
    }

    LLUtils::PointI32 TestApp::ResolveOffset(const LLUtils::PointI32& point)
    {
        using namespace LLUtils;
        PointF64 imageSize = GetImageSize(true);
        PointF64 clientSize = fWindow.GetClientSize();
        PointF64 offset = static_cast<PointF64>(point);
        const Serialization::UserSettingsData& settings = fSettings.getUserSettings();
        
        offset.x = CalculateOffset(clientSize.x, imageSize.x, offset.x, settings.zoomScrollState.Margins.x);
        offset.y = CalculateOffset(clientSize.y, imageSize.y, offset.y, settings.zoomScrollState.Margins.x);
        return static_cast<PointI32>(offset);
    }

    void TestApp::TransformImage(OIV_AxisAlignedRTransform transform)
    {
        OIVCommands::TransformImage(ImageHandleDisplayed, transform);
        OIVCommands::Refresh();
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
        LLUtils::RectF64 imageRect;
        ResultCode result = OIVCommands::GetImageCoordinates(fSelectionRect, imageRect);

        if (result == RC_Success)
        {
            LLUtils::RectI32 imageRectInt = {{ (int)imageRect.p0.x,(int)imageRect.p0.y}
                ,{ (int)imageRect.p1.x,(int)imageRect.p1.y } };
            
            ImageHandle croppedHandle;
            result = OIVCommands::CropImage(ImageHandleDisplayed, imageRectInt, croppedHandle);
            if (result == RC_Success)
            {

                struct unloadImage
                {
                    ImageHandle handle;
                    ~unloadImage()
                    {
                        if (handle != ImageNullHandle)
                            OIVCommands::UnloadImage(handle);
                                
                    }
                } handle{ croppedHandle};


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
    }

    void TestApp::CropVisibleImage()
    {
        LLUtils::RectF64 imageRect;
        ResultCode result = OIVCommands::GetImageCoordinates(fSelectionRect, imageRect);

        if (result == RC_Success)
        {
            LLUtils::RectI32 imageRectInt = { { (int)imageRect.p0.x,(int)imageRect.p0.y }
            ,{ (int)imageRect.p1.x,(int)imageRect.p1.y } };

            ImageHandle croppedHandle;
            result = OIVCommands::CropImage(ImageHandleDisplayed, imageRectInt, croppedHandle);
            if (result == RC_Success)
            {
                fImageBeingOpened = fOpenedImage;
                fImageBeingOpened.imageHandle = croppedHandle;
                fImageBeingOpened.width = 7;
                fImageBeingOpened.height= 7;
                
            }
        }
        FinalizeImageLoadThreadSafe(result);

    }

    bool TestApp::HandleWinMessageEvent(const Win32::EventWinMessage* evnt)
    {
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
        break;
        case Win32::UserMessage::PRIVATE_WN_NOTIFY_LOADED:
            FinalizeImageLoadThreadSafe(static_cast<ResultCode>( evnt->message.wParam));
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
        const bool IsLeftCaptured = mouseState.IsCaptured(MouseState::Button::Left);
        const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_Released;
        const bool IsRightPressed = evnt->GetButtonEvent(MouseState::Button::Right) == MouseState::EventType::ET_Pressed;
        const bool IsLeftPressed = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_Pressed;
        const bool IsMiddlePressed = evnt->GetButtonEvent(MouseState::Button::Middle) == MouseState::EventType::ET_Pressed;
        const bool IsLeftDoubleClick = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::ET_DoublePressed;

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
                Pan(evnt->DeltaX, evnt->DeltaY);
        }

        LONG wheelDelta = evnt->DeltaWheel;

        if (wheelDelta != 0)
        {
            if (IsRightCatured || isMouseUnderCursor)
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
        
        if (isMouseUnderCursor)
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
}
