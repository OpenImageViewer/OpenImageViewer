#pragma once
#include <memory>
#include <windows.h>
#include <LLUtils/Point.h>
#include <LLUtils/StopWatch.h>
#include <Win32/HighPrecisionTimer.h>


namespace OIV
{
    class AutoScroll
    {
    public:

        using OnScrollFunction = std::function< void(const LLUtils::PointF64&)>;

        struct ScrollMetrics
        {
            uint16_t deadZoneRadius = 10; // in pixels
            double speedInFactorIn = 0.9;
            double speedFactorOut = 1.7;
            uint16_t speedFactorRange = 40; // in pixels
            uint16_t maxSpeed = 5000; // in pixels per second.
        };
        
        struct CreateParams
        {
            HWND windowHandle;
            DWORD windowMessage;
            OnScrollFunction scrollFunc;
        };
        
        AutoScroll(const CreateParams& createParams);
        bool IsAutoScrolling() const { return fAutoScrolling; }
        void SetScrollMetrics(const ScrollMetrics& scrollMetrics);
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
        
        ScrollMetrics fScrollMetrics{};

        bool fAutoScrolling = false;
        LLUtils::PointI32 fAutoScrollPosition = 0;
        LLUtils::StopWatch fAutoScrollStopWatch;
        ::Win32::HighPrecisionTimer fTimer = ::Win32::HighPrecisionTimer(std::bind(&AutoScroll::OnScroll, this));
        CreateParams fCreateParams {};
        
#pragma endregion
    };

    typedef std::unique_ptr<AutoScroll> AutoScrollUniquePtr;

}
