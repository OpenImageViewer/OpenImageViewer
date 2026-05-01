#include "AutoScroll.h"
#include "win32/UserMessages.h"
#include <Win32/Win32Helper.h>


namespace OIV
{
    AutoScroll::AutoScroll(const CreateParams& createParams) : fCreateParams(createParams)
    {
    }
    
    void AutoScroll::PerformAutoScroll()
    {
        using namespace LLUtils;
        fAutoScrollStopWatch.Stop();
        const double elapsed = static_cast<double>( fAutoScrollStopWatch.GetElapsedTimeReal(StopWatch::TimeUnit::Milliseconds));
        fAutoScrollStopWatch.Start();
        ScrollPointType deltaFromScrollPosition = static_cast<ScrollPointType>(fAutoScrollPosition - GetMousePosition()) ;
        ScrollPointType deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x <= fScrollMetrics.deadZoneRadius ? 0 : (deltaAbs.x - fScrollMetrics.deadZoneRadius);
        deltaAbs.y = deltaAbs.y <= fScrollMetrics.deadZoneRadius ? 0 : (deltaAbs.y - fScrollMetrics.deadZoneRadius);

        auto factorOut = fScrollMetrics.speedFactorOut;
        auto factorIn = fScrollMetrics.speedInFactorIn;
        
        auto powerX = std::min<double>(factorOut, (factorOut - factorIn) / fScrollMetrics.speedFactorRange * deltaAbs.x + factorIn);
        auto powerY = std::min<double>(factorOut, (factorOut - factorIn) / fScrollMetrics.speedFactorRange * deltaAbs.y + factorIn);

        auto movementSpeedX = std::min<double>(fScrollMetrics.maxSpeed, deltaAbs.x == 0 ? 0 : std::pow(deltaAbs.x, powerX));
        auto movementSpeedY = std::min<double>(fScrollMetrics.maxSpeed, deltaAbs.y == 0 ? 0 : std::pow(deltaAbs.y, powerY));

        LLUtils::PointF64 movement{ movementSpeedX  , movementSpeedY };


        PointF64 scrollAmount = movement * (elapsed * 0.001) * deltaFromScrollPosition.Sign();
        
        fCreateParams.scrollFunc(static_cast<PointF64>(scrollAmount));
    }

    void AutoScroll::OnScroll()
    {
        PerformAutoScroll();
    }

    LLUtils::PointI32 AutoScroll::GetMousePosition()
    {
        return static_cast<LLUtils::PointI32>(::Win32::Win32Helper::GetMouseCursorPosition(fCreateParams.windowHandle));
    }


    void AutoScroll::ToggleAutoScroll()
    {
        fAutoScrolling = !fAutoScrolling;
        if (fAutoScrolling == true)
        {
            // start auto scroll
            fTimer.SetDueTime(fScrollTimeDelay);
            fTimer.SetRepeatInterval(fScrollTimeDelay);
            fAutoScrollPosition = GetMousePosition();
        }
        else
        {
            // Stop auto scroll
            fAutoScrollPosition = LLUtils::PointI32::Zero;
        }
        
        fTimer.Enable(fAutoScrolling);
    }

}
