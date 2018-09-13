#include "AutoScroll.h"
#include "win32/UserMessages.h"


namespace OIV
{

    AutoScroll::AutoScroll(Win32::Win32WIndow * window, OnScrollFunction scrollFunc) :
          fWindow(window)
        , fOnScroll(scrollFunc)
    {
        
    }
    
    void AutoScroll::PerformAutoScroll(const Win32::EventWinMessage* evnt)
    {
        ////There may be pending message 
        //TODO: wait  for processing all messages after  kill timer
        if (fAutoScrolling == false)
            return;

        using namespace LLUtils;
        fAutoScrollStopWatch.Stop();
        const long double elapsed = fAutoScrollStopWatch.GetElapsedTimeReal(StopWatch::TimeUnit::Milliseconds);
        fAutoScrollStopWatch.Start();
        ScrollPointType deltaFromScrollPosition = static_cast<ScrollPointType>(fAutoScrollPosition - PointI32(fWindow->GetMousePosition()) );
        UpdateCursorFromDeltaVector(deltaFromScrollPosition);

        ScrollPointType deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x < fAutoScrollDeadZone ? 0 : (deltaAbs.x - (fAutoScrollDeadZone - 1));
        deltaAbs.y = deltaAbs.y < fAutoScrollDeadZone ? 0 : (deltaAbs.y - (fAutoScrollDeadZone - 1));
        PointF64 autoScrollAmount = deltaAbs * (elapsed * fScrollSpeed * 0.001) * deltaFromScrollPosition.Sign();
        fOnScroll(autoScrollAmount);
    }


    void AutoScroll::OnScroll()
    {
        fWindow->SendMessage(Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL, 0, 0);
    }

    void AutoScroll::ToggleAutoScroll()
    {
        fAutoScrolling = !fAutoScrolling;
        if (fAutoScrolling == true)
        {
            // start auto scroll
            fTimer.SetDelay(fScrollTimeDelay);
            fAutoScrollPosition = fWindow->GetMousePosition();
        }
        else
        {
            // Stop auto scroll
            fAutoScrollPosition = LLUtils::PointI32::Zero;
            fWindow->SetCursorType(Win32::Win32WIndow::CursorType::SystemDefault);

        }
        
        fTimer.Enable(fAutoScrolling);
    }

    void AutoScroll::UpdateCursorFromDeltaVector(LLUtils::PointF64  aDeltaVector)
    {

        const double PI = 3.14159265358979323846;

        double rad = atan2(-aDeltaVector.y, aDeltaVector.x);
        double deg = (rad * 180) / PI + 180;
        const int numDirections = 8;
        const int step = 360 / numDirections;

        int index = (static_cast<int>(deg) + step / 2) % 360 / step;
        fWindow->SetCursorType((Win32::Win32WIndow::CursorType)((int)index + 2));
    }


}
