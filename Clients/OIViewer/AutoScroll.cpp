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
        ////There may be pending message 
        //TODO: wait  for processing all messages after  kill timer
        if (fAutoScrolling == false)
            return;

        using namespace LLUtils;
        fAutoScrollStopWatch.Stop();
        const double elapsed = static_cast<double>( fAutoScrollStopWatch.GetElapsedTimeReal(StopWatch::TimeUnit::Milliseconds));
        fAutoScrollStopWatch.Start();
        ScrollPointType deltaFromScrollPosition = static_cast<ScrollPointType>(fAutoScrollPosition - GetMousePosition()) ;
        ScrollPointType deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x < fAutoScrollDeadZone ? 0 : (deltaAbs.x - (fAutoScrollDeadZone - 1));
        deltaAbs.y = deltaAbs.y < fAutoScrollDeadZone ? 0 : (deltaAbs.y - (fAutoScrollDeadZone - 1));
        PointF64 autoScrollAmount = deltaAbs * (elapsed * fScrollSpeed * 0.001) * deltaFromScrollPosition.Sign();
        
        fCreateParams.scrollFunc(autoScrollAmount);
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
