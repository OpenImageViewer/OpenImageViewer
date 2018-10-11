#pragma once
#include <windows.h>
#include <Point.h>
namespace OIV
{
    enum class WindowSetPosOp
    {
          None
        , Resize
        , Move
        , Zorder
        , RefreshFrame

    };

    class Win32Helper
    {
    public:
        static bool IsKeyPressed(DWORD virtualKey)
        {
            return (GetKeyState(virtualKey) & static_cast<USHORT>(0x8000)) != 0;

        }

        static bool IsKeyToggled(DWORD virtualKey)
        {
            return (GetKeyState(virtualKey) & static_cast<USHORT>(0x0001)) != 0;

        }

        static void MessageLoop()
        {
            MSG msg;
            BOOL bRet;
            while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
            {
                if (bRet == -1)
                {
                    // handle the error and possibly exit
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }


        static POINT GetMouseCursorPosition(HWND handle)
        {
            POINT clientMousePos = GetMouseCursorPosition();
            ScreenToClient(handle, &clientMousePos);
            return clientMousePos;
        }

        static POINT GetMouseCursorPosition()
        {
            POINT mousePos;
            GetCursorPos(&mousePos);
            return mousePos;
        }

        static void MoveMouse(LLUtils::PointI32 point)
        {
            POINT mousePos;
            ::GetCursorPos(&mousePos);
            ::SetCursorPos(mousePos.x + point.x, mousePos.y + point.y);
        }

        static SIZE GetRectSize(const RECT& rect)
        {
            return{ rect.right - rect.left , rect.bottom - rect.top };
        }


        static std::wstring OpenFile(HWND ownerWindow)
        {        
            wchar_t filename[MAX_PATH];

            OPENFILENAME ofn;
            ZeroMemory(&filename, sizeof(filename));
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = ownerWindow;  // If you have a window to center over, put its HANDLE here
            ofn.lpstrFilter = L"Any File\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Open a file";
            ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn))
                return filename;
            else
                return std::wstring();
        }



        static UINT GetFlagsForWindowSetPosOp(WindowSetPosOp op)
        {
            static const UINT BaseFlags =
                0
                | SWP_NOMOVE
                | SWP_NOSIZE
                | SWP_NOZORDER
                | SWP_NOOWNERZORDER
                | SWP_NOACTIVATE
                | SWP_NOCOPYBITS
                | SWP_NOREPOSITION
                | SWP_NOREDRAW
                | SWP_NOSENDCHANGING
                | SWP_DEFERERASE
                ;

            switch (op)
            {
            case WindowSetPosOp::None:
                return 0;
            case WindowSetPosOp::Move:
                return BaseFlags & ~SWP_NOMOVE;
                break;
            case WindowSetPosOp::RefreshFrame:
                return BaseFlags | SWP_FRAMECHANGED;
                break;
            case WindowSetPosOp::Resize:
                return BaseFlags & ~SWP_NOSIZE;
                break;
            case WindowSetPosOp::Zorder:
                return BaseFlags & ~SWP_NOZORDER;
                break;
            default:
                return 0;
            }
        }
    };
}
