#include "AutoScroll.h"


namespace OIV
{
#pragma region AutoScroll
    void AutoScroll::OnAutoScrollTimer(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
    {
        reinterpret_cast<Win32::Win32WIndow*>(dwUser)->SendMessage(PRIVATE_WN_AUTO_SCROLL, 0, 0);
    }

    void AutoScroll::PerformAutoScroll(const Win32::EventWinMessage* evnt)
    {
        fAutoScrollStopWatch.Stop();
        const long double elapsed = fAutoScrollStopWatch.GetElapsedTime(StopWatch::TimeUnit::Milliseconds);
        fAutoScrollStopWatch.Start();
        PointF32 deltaFromScrollPosition = static_cast<PointF32>(PointI32(fWindow->GetMousePosition()) - fAutoScrollPosition);

        PointF32 deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x < fAutoScrollMercyZone ? 0 : (deltaAbs.x - (fAutoScrollMercyZone - 1));
        deltaAbs.y = deltaAbs.y < fAutoScrollMercyZone ? 0 : (deltaAbs.y - (fAutoScrollMercyZone - 1));
        fAutoScrollPanning += deltaAbs * (elapsed / 100.0f) * deltaFromScrollPosition.Sign();

        if (fAutoScrollPanning.Abs().IsZero() == false)
        {
            PointI32 actualPanning = static_cast<PointI32>(fAutoScrollPanning);
            fOnScroll(actualPanning.x, actualPanning.y);
            
            fAutoScrollPanning -= static_cast<PointF32>(actualPanning);
        }
    }

    void AutoScroll::ToggleAutoScroll()
    {
        fAutoScrolling = !fAutoScrolling;

        if (fAutoScrolling)
        {
            fAutoScrollTimerID = timeSetEvent(5, 0, OnAutoScrollTimer, reinterpret_cast<DWORD_PTR>(fWindow), TIME_PERIODIC);
            fAutoScrollPosition = fWindow->GetMousePosition();
        }
        else
        {
            timeKillEvent(fAutoScrollTimerID);
            fAutoScrollPosition = PointI32::Zero;
        }
    }

#pragma endregion 
}
