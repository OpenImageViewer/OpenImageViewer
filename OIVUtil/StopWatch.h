#pragma once
#include <chrono>

namespace OIV
{
    class StopWatch
    {
        
    public:
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


        long long GetElapsedNanoSeconds() const
        {
            std::chrono::high_resolution_clock::time_point end = fIsRunning ? std::chrono::high_resolution_clock::now() : fEnd;
            return (end - fStart).count();
        }


        double long GetElapsedMicroSeconds() const
        {
            return GetElapsedNanoSeconds() / static_cast<long double>(1000.0 * 1000.0);
        }
        
    private:
        std::chrono::high_resolution_clock::time_point fStart;
        std::chrono::high_resolution_clock::time_point fEnd;
        bool fIsRunning = false;
    };
}
