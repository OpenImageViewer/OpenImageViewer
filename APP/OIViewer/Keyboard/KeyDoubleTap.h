#pragma once
#include <windows.h>
#include <chrono>
class KeyDoubleTap
{
    static constexpr int MaxDelayBetweenTaps = 320;
    std::chrono::high_resolution_clock::time_point fLastTap;
public:
    std::function<void()> callback;
    void SetState(bool down)
    {
        const bool up = !down;
        using namespace std::chrono;

        if (up)
        {
            high_resolution_clock::time_point now = high_resolution_clock::now();
            if (std::chrono::duration_cast<milliseconds>(now - fLastTap).count() < MaxDelayBetweenTaps)
            {
                //trigger double tap.
                callback();
                //MessageBox(nullptr, L"12", L"12", MB_OK);
                fLastTap = high_resolution_clock::time_point::min();
            }
            fLastTap = high_resolution_clock::now();
        }
    }
};