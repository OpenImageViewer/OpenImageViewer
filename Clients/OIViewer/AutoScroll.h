#pragma once
#include <memory>
#include <windows.h>
#include <LLUtils/Point.h>
#include <LLUtils/StopWatch.h>
#include "win32/HighPrecisionTimer.h"


namespace OIV
{
    class AutoScroll
    {
    public:

        using OnScrollFunction = std::function< void(const LLUtils::PointF64&)>;
        
        struct CreateParams
        {
            HWND windowHandle;
            DWORD windowMessage;
            OnScrollFunction scrollFunc;
        };


        
        AutoScroll(const CreateParams& createParams);
        bool IsAutoScrolling()const { return fAutoScrolling; }

        LLUtils::PointI32 GetMousePosition();
        void ToggleAutoScroll();
        void PerformAutoScroll();

#pragma region Private member methods
        private:
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
        Win32::HighPrecisionTimer fTimer = Win32::HighPrecisionTimer(std::bind(&AutoScroll::OnScroll, this));
        HANDLE fAutoScrollTimerID = nullptr;
        CreateParams fCreateParams = {};
        
#pragma endregion
    };

    typedef std::unique_ptr<AutoScroll> AutoScrollUniquePtr;

}
