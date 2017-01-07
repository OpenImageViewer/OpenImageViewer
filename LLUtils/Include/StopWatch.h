#pragma once
#include <chrono>

namespace LLUtils
{
    class StopWatch
    {
    public:
        typedef long double time_type;
        enum TimeUnit { Undefined = 0, NanoSeconds = 1, MicroSeconds = 2, Milliseconds = 3, Seconds = 4};
    private:

        const time_type multiplierMap[5] = {0L,1,0.001L,0.000001L,0.000000001L};
    public:

        StopWatch(bool start = false)
        {
            if (start)
                Start();
        }

        void Start()
        {
            fStart = std::chrono::high_resolution_clock::now();
            fIsRunning = true;
        }
        void Stop()
        {
            fIsRunning = false;
            fEnd = std::chrono::high_resolution_clock::now();
        }

        // Get the elapsed time in nanoseconds.
        long long GetElapsedTime() const
        {
            return GetElapsedNanoSeconds();
        }


        time_type GetElapsedTime(TimeUnit timeUnit) const
        {
            return GetElapsedNanoSeconds() * multiplierMap[timeUnit];
        }


    private:
        long long GetElapsedNanoSeconds() const
        {
            std::chrono::high_resolution_clock::time_point end = fIsRunning ? std::chrono::high_resolution_clock::now() : fEnd;
            return (end - fStart).count();
        }
    private:
        std::chrono::high_resolution_clock::time_point fStart;
        std::chrono::high_resolution_clock::time_point fEnd;
        bool fIsRunning = false;
    };
}
