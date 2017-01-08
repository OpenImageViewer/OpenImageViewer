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
        using namespace LLUtils;
        fAutoScrollStopWatch.Stop();
        const long double elapsed = fAutoScrollStopWatch.GetElapsedTime(StopWatch::TimeUnit::Milliseconds);
        fAutoScrollStopWatch.Start();
        ScrollPointType deltaFromScrollPosition = static_cast<ScrollPointType>(PointI32(fWindow->GetMousePosition()) - fAutoScrollPosition);

        ScrollPointType deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x < fAutoScrollDeadZone ? 0 : (deltaAbs.x - (fAutoScrollDeadZone - 1));
        deltaAbs.y = deltaAbs.y < fAutoScrollDeadZone ? 0 : (deltaAbs.y - (fAutoScrollDeadZone - 1));
        fAutoScrollPanning += deltaAbs * (elapsed * fScrollSpeed * 0.001) * deltaFromScrollPosition.Sign();

        if (fAutoScrollPanning.Abs() != ScrollPointType::Zero)
        {
            PointI32 actualPanning = static_cast<PointI32>(fAutoScrollPanning);
            fOnScroll(actualPanning);
            
            fAutoScrollPanning -= static_cast<ScrollPointType>(actualPanning);
        }
    }

    void AutoScroll::ToggleAutoScroll()
    {
        fAutoScrolling = !fAutoScrolling;

        if (fAutoScrolling)
        {
            fAutoScrollTimerID = timeSetEvent(fScrollTimeDelay, 0, OnAutoScrollTimer, reinterpret_cast<DWORD_PTR>(fWindow), TIME_PERIODIC);
            fAutoScrollPosition = fWindow->GetMousePosition();
        }
        else
        {
            timeKillEvent(fAutoScrollTimerID);
            fAutoScrollPosition = LLUtils::PointI32::Zero;
        }
    }

#pragma endregion 
}
