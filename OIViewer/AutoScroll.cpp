#include "AutoScroll.h"
#include "win32/UserMessages.h"


namespace OIV
{
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

        ScrollPointType deltaAbs = deltaFromScrollPosition.Abs();
        deltaAbs.x = deltaAbs.x < fAutoScrollDeadZone ? 0 : (deltaAbs.x - (fAutoScrollDeadZone - 1));
        deltaAbs.y = deltaAbs.y < fAutoScrollDeadZone ? 0 : (deltaAbs.y - (fAutoScrollDeadZone - 1));
        PointF64 autoScrollAmount = deltaAbs * (elapsed * fScrollSpeed * 0.001) * deltaFromScrollPosition.Sign();
        fOnScroll(autoScrollAmount);
    }

    VOID CALLBACK AutoScroll::OnScroll(
        _In_ PVOID   lpParameter,
        _In_ BOOLEAN TimerOrWaitFired
    )
    {
        reinterpret_cast<Win32::Win32WIndow*>(lpParameter)->SendMessage(Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL, 0, 0);
    }

    void AutoScroll::ToggleAutoScroll()
    {

        if (fAutoScrolling == false)
        {
            if (
            CreateTimerQueueTimer(
                  &fAutoScrollTimerID
                , nullptr//_In_opt_ HANDLE              TimerQueue,
                , OnScroll//_In_     WAITORTIMERCALLBACK Callback,
                , reinterpret_cast<PVOID>(fWindow) //_In_opt_ PVOID               Parameter,
                , 0//_In_     DWORD               DueTime,
                , fScrollTimeDelay//_In_     DWORD               Period,
                , WT_EXECUTEINTIMERTHREAD//_In_     ULONG               Flags
            ) == FALSE )
            {
                LL_EXCEPTION_SYSTEM_ERROR("Could not create timer");
            }

            fAutoScrollPosition = fWindow->GetMousePosition();
        }
        else
        {
            DeleteTimerQueueTimer(nullptr, fAutoScrollTimerID, nullptr);
            fAutoScrollTimerID = nullptr;
            fAutoScrollPosition = LLUtils::PointI32::Zero;
        }

        fAutoScrolling = !fAutoScrolling;
    }
}
