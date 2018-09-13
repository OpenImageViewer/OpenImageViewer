#pragma once
#include <windows.h>
#include <Exception.h>
namespace OIV
{
    namespace Win32
    {
        class HighPrecisionTimer
        {
        public:

            using Callback = std::function<void()>;


            HighPrecisionTimer(Callback callback)
                :
                fCallback(callback)
            {
            }



            void SetDelay(DWORD delay)
            {
                if (fDelay != delay)
                {
                    fDelay = delay;
                    if (fEnabled == true)
                    {
                        Enable(false);
                        Enable(true);
                    }
                }
            }

            void Enable(bool enable)
            {
                if (enable != fEnabled)
                {
                    fEnabled = enable;

                    if (fEnabled)
                    {
                        if (
                            CreateTimerQueueTimer(
                                &fTimerID
                                , nullptr//_In_opt_ HANDLE              TimerQueue,
                                , OnTimer//_In_     WAITORTIMERCALLBACK Callback,
                                , reinterpret_cast<PVOID>(this) //_In_opt_ PVOID               Parameter,
                                , 0//_In_     DWORD               DueTime,
                                , fDelay//_In_     DWORD               Period,
                                , WT_EXECUTEINTIMERTHREAD//_In_     ULONG               Flags
                            ) == FALSE)
                        {
                            LL_EXCEPTION_SYSTEM_ERROR("Could not create timer");
                        }
                    }
                    else
                    {
                        DeleteTimerQueueTimer(nullptr, fTimerID, nullptr);
                        fTimerID = nullptr;
                    }
                }
            }

        private:
            static VOID CALLBACK OnTimer(
                _In_ PVOID   lpParameter,
                _In_ BOOLEAN TimerOrWaitFired
            )
            {
                reinterpret_cast<HighPrecisionTimer*>(lpParameter)->fCallback();
            }

        private:
            bool fEnabled = false;
            HANDLE fTimerID = nullptr;
            Callback fCallback;
            DWORD fDelay = 50;
        };
    }
}