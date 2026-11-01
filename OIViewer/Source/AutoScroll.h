#pragma once

#include <LWS/Window.hpp>
#include <memory>
#include <LLUtils/Point.h>
#include <LLUtils/StopWatch.h>
#include <LWS/Timer.hpp>

namespace OIV
{
    class AutoScroll
    {
      public:

        using OnScrollFunction = std::function<void(const LLUtils::PointF64&)>;

        struct ScrollMetrics
        {
            uint16_t deadZoneRadius   = 10;  // in pixels
            double speedInFactorIn    = 0.9;
            double speedFactorOut     = 1.7;
            uint16_t speedFactorRange = 40;    // in pixels
            uint16_t maxSpeed         = 5000;  // in pixels per second.
        };

        struct CreateParams
        {
            LWS::Window* window{};
            OnScrollFunction scrollFunc;
        };

        AutoScroll(const CreateParams& createParams);
        bool IsAutoScrolling() const { return fAutoScrolling; }
        LLUtils::PointI32 GetMousePosition();
        void ToggleAutoScroll();
        void PerformAutoScroll();
        void SetDeadZoneRadius(int32_t val) { fScrollMetrics.deadZoneRadius = val; }
        void SetSpeedFactorIn(double val) { fScrollMetrics.speedInFactorIn = val; }
        void SetSpeedFactorOut(double val) { fScrollMetrics.speedFactorOut = val; }
        void SetSpeedFactorRange(int32_t val) { fScrollMetrics.speedFactorRange = val; }
        void SetMaxSpeed(int32_t val) { fScrollMetrics.maxSpeed = val; }

#pragma region Private member methods

      private:

        void OnScroll();
#pragma endregion

        typedef LLUtils::PointF64 ScrollPointType;

#pragma region Private member fields

      private:

        // Scroll paramaters
        uint16_t fScrollTimeDelay = 1;  // in milliseconds

        ScrollMetrics fScrollMetrics{};

        bool fAutoScrolling                   = false;
        LLUtils::PointI32 fAutoScrollPosition = 0;
        LLUtils::StopWatch fAutoScrollStopWatch;
        LWS::HighPrecisionTimer fTimer;
        CreateParams fCreateParams{};

#pragma endregion
    };

    typedef std::unique_ptr<AutoScroll> AutoScrollUniquePtr;

}  // namespace OIV
