#pragma once
#include <windows.h>
#include "Point.h"
#include "StopWatch.h"
#include "win32/Win32Window.h"
#include <memory>

namespace OIV
{
    class AutoScroll
    {
    public:
        typedef std::function< void(int32_t, int32_t) > OnScrollFunction;
        static const UINT PRIVATE_WN_AUTO_SCROLL = WM_USER + 1;
        AutoScroll(Win32::Win32WIndow* window, OnScrollFunction scrollFunc) :
               fWindow(window)
             , fOnScroll(scrollFunc)

        {
            
        }

        void ToggleAutoScroll();
        void PerformAutoScroll(const Win32::EventWinMessage* evnt);
#pragma region Private member methods
        private:
        static void OnAutoScrollTimer(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
#pragma endregion

    
#pragma region Private member fields
    private:
        bool fAutoScrolling = false;
        LLUtils::PointI32 fAutoScrollPosition = LLUtils::PointI32::Zero;
        LLUtils::StopWatch fAutoScrollStopWatch;
        LLUtils::PointF32 fAutoScrollPanning = LLUtils::PointF32::Zero;
        uint8_t fAutoScrollMercyZone = 20;
        MMRESULT fAutoScrollTimerID = 0;
        Win32::Win32WIndow* fWindow;
        OnScrollFunction fOnScroll;
#pragma endregion
    };

    typedef std::unique_ptr<AutoScroll> AutoScrollUniquePtr;

}
