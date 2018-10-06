#pragma once
#include <chrono>

namespace LLUtils
{
    template <typename time_type_real = long double
    , typename time_type_integer = int64_t
    , typename base_clock = std::chrono::high_resolution_clock>
    
    
    class StopWatchBasic
    {
    public:
        enum TimeUnit { Undefined = 0, NanoSeconds = 1, MicroSeconds = 2, Milliseconds = 3, Seconds = 4, TimeUnitCount};
        typedef time_type_integer  time_type_integer;
        typedef time_type_real  time_type_real;

    private:
        using base_clock_rep = typename base_clock::rep;
        using base_clock_time_point = typename base_clock::time_point;
        
        inline static const time_type_real multiplierMapReal      [TimeUnitCount] = { 0, 1 , 0.001 , 0.000001 ,0.000000001};
        inline static const base_clock_rep multiplierMapInteger   [TimeUnitCount] = { 0, 1 , 1000  , 1000000  ,1000000000 };
        
    public:
        StopWatchBasic(bool start = false)
        {
            if (start)
                Start();
        }

        void Start()
        {
            fStart = base_clock::now();
            fIsRunning = true;
        }
        void Stop()
        {
            fIsRunning = false;
            fEnd = base_clock::now();
        }

        time_type_integer GetElapsedTimeInteger(TimeUnit timeUnit) const
        {
            return static_cast<time_type_integer>(GetElapsedNanoSeconds() / multiplierMapInteger[timeUnit]);
        }

        time_type_real GetElapsedTimeReal(TimeUnit timeUnit) const
        {
            return static_cast<time_type_real>(GetElapsedNanoSeconds() * multiplierMapReal [timeUnit]);
        }
    private:

        base_clock_rep GetElapsedNanoSeconds() const
        {
            
            base_clock_time_point end = fIsRunning ? base_clock::now() : fEnd;
            return (end - fStart).count();
        }

    private:
        base_clock_time_point fStart;
        base_clock_time_point fEnd;
        bool fIsRunning = false;
    };

    typedef StopWatchBasic<> StopWatch;
}
