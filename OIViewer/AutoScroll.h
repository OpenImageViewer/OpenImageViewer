#pragma once
#include <memory>

#include <windows.h>
#include "Point.h"
#include "StopWatch.h"
#include "win32/Win32Window.h"
#include "win32/HighPrecisionTimer.h"


namespace OIV
{
    class AutoScroll
    {
    public:
        typedef std::function< void(const LLUtils::PointF64&) > OnScrollFunction;
        AutoScroll(Win32::MainWindow* window, OnScrollFunction scrollFunc);

        void ToggleAutoScroll();
        void PerformAutoScroll(const Win32::EventWinMessage* evnt);

#pragma region Private member methods
        private:
        void UpdateCursorFromDeltaVector(LLUtils::PointF64 aDeltaVector);
        void OnScroll();
#pragma endregion
        
        typedef LLUtils::PointF64 ScrollPointType;
   
#pragma region Private member fields
    private:
        //Scroll paramaters
        uint16_t fScrollTimeDelay = 1; // in milliseconds
        uint8_t fAutoScrollDeadZone = 20; // in pixels
        uint16_t fScrollSpeed = 5; // pixels per second per step

        bool fAutoScrolling = false;
        LLUtils::PointI32 fAutoScrollPosition = 0;
        LLUtils::StopWatch fAutoScrollStopWatch;
        Win32::HighPrecisionTimer fTimer = std::bind(&AutoScroll::OnScroll, this);
        HANDLE fAutoScrollTimerID = nullptr;
        Win32::MainWindow* fWindow;
        OnScrollFunction fOnScroll;
        
#pragma endregion
    };

    typedef std::unique_ptr<AutoScroll> AutoScrollUniquePtr;

}
