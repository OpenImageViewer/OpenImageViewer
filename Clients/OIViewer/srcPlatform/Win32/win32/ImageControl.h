#pragma once
#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <Win32/Win32Window.h>
#include "ImageLIst.h"
namespace OIV
{
    namespace Win32

    {
        class ImageControl : public ::Win32::Win32Window
        {
        public:

            ImageControl()
            {
                AddEventListener(std::bind(&ImageControl::HandleWindwMessage, this, std::placeholders::_1));
            }

            void SetImagePos(int pos)
            {
                fImageList.SetPos(pos);
            }


            ImageList& GetImageList() { return fImageList; }

            void RefreshScrollInfo()
            {
                const size_t deltaElements = fImageList.GetNumberOfElements() - fImageList.GetNumberOfDisplayedElements();
                SCROLLINFO si{};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_ALL;
                si.nMin = 0;
                si.nMax = static_cast<int>((std::max)(static_cast<size_t>(0), deltaElements));
                si.nPage = 1;
                si.nPos = (std::min)(GetScrollPos(GetHandle(), SB_VERT), si.nMax);
                SetScrollInfo(GetHandle(), SB_VERT, &si, TRUE);
                SetImagePos(si.nPos);
                InvalidateRect(GetHandle(), nullptr, TRUE);
            }

            bool HandleWindwMessage(const ::Win32::Event* evnt1)
            {
                bool handled = true;

                const ::Win32::EventWinMessage* evnt = dynamic_cast<const ::Win32::EventWinMessage*>(evnt1);
                if (evnt == nullptr)
                    return false;
            
                const ::Win32::WinMessage& msg = evnt->message;
                switch (msg.message)
                {
                case WM_LBUTTONDOWN:
                {
                    int xPos = GET_X_LPARAM(msg.lParam);
                    int yPos = GET_Y_LPARAM(msg.lParam);
                    fImageList.MouseClick(xPos, yPos);
                }

                break;
                case WM_VSCROLL:
                {
                    INT minRange, maxRange;
                    GetScrollRange(msg.hWnd, SB_VERT, &minRange, &maxRange);
                    int oldPos = GetScrollPos(msg.hWnd, SB_VERT);
                    int newPos = oldPos;
                    switch (LOWORD(msg.wParam))
                    {
                    case SB_PAGEDOWN:
                        newPos++;
                        break;
                    case SB_PAGEUP:
                        newPos--;
                        break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION:
                    {
                        newPos = HIWORD(msg.wParam);
                    }

                    break;
                    case SB_ENDSCROLL:
                        break;
                    }
                    newPos = std::clamp(newPos, minRange, maxRange);

                    if (newPos != oldPos)
                    {
                        SetImagePos(newPos);
                        SetScrollPos(msg.hWnd, SB_VERT, newPos, TRUE);
                        InvalidateRect(msg.hWnd, nullptr, TRUE);
                    }


                }
                break;
                case WM_CREATE:
                {
                    fImageList.SetTarget(GetHandle());
                }
                break;
                case WM_PAINT:
                    fImageList.Draw();
                    return true;
                    break;
                case WM_MOUSEWHEEL:
                    //case WM_MOUSEHWHEEL:
                {
                    //MessageBox(0, L"aa", L"aa", MB_OK);
                    int zDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA;
                    DWORD direction = zDelta < 0 ? SB_PAGEDOWN : SB_PAGEUP;
                    SendMessage(WM_VSCROLL, direction, 0);

                }
                break;

                case WM_SIZE:
                {
                    RefreshScrollInfo();
                }
                break;
                default:
                    handled = false;

                }
                
                return handled;

            }
          
        private:
            ImageList fImageList;
        };
    }
}