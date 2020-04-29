#include "Timer.h"
#include "Exception.h"

//-----------------------------------------------------
//Timer manager implementation
namespace OIV::Win32
{
    
    TimerManager::TimerIDType TimerManager::RegisterTimer(const Timer& timer)
    {
        TimerIDType id = fUniqueIdProvider.Acquire();

        auto it = fMapTimerIdToTimer.emplace(id, &timer);

        if (it.second == false)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "Timer id already registered");

        return id;
    }



    void TimerManager::UnRegisterTimer(TimerIDType timerID)
    {
        auto it = fMapTimerIdToTimer.find(timerID);
        if (it == fMapTimerIdToTimer.end())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Timer id not found in data structure");

        fMapTimerIdToTimer.erase(it);
        fUniqueIdProvider.Release(timerID);
    }

    void TimerManager::Timerproc(
        HWND hwnd,        // handle to window for timer messages 
        UINT message,     // WM_TIMER message 
        UINT idTimer,     // timer identifier 
        DWORD dwTime)     // current system time 
    {
        TimerManager& _this = TimerManager::GetSingleton();

        auto it = _this.fMapTimerIdToTimer.find(idTimer);
        if (it == _this.fMapTimerIdToTimer.end())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Timer id not found in data structure");

        const Timer* timer = it->second;
        timer->fCallback();
    }
}
//-----------------------------------------------------