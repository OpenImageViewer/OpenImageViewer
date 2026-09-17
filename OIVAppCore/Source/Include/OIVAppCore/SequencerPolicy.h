#pragma once

#include <LLUtils/StringDefs.h>
#include <OIVAppCore/CommandManager.h>
#include <OIVShared/AdaptiveMotion.h>
#include <OIVShared/UnitFormatter.h>

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace IMCodec
{
    class Image;
}

namespace OIV
{
    // Animation uses one multiplier for every frame. Source delays retain their historical 5 ms
    // normalization; the configurable playback floor is applied after scaling, independently of it.
    class SequencerPolicy
    {
      public:

        using Clock = std::chrono::steady_clock;
        // Largest finite interval shared by Win32 SetTimer and the Wayland timer. UINT32_MAX
        // means "never fire" on Wayland, and Win32 clamps intervals above INT32_MAX.
        static constexpr uint32_t MaxTimerIntervalMs = 0x7fffffff;

        struct Settings
        {
            uint32_t minFrameIntervalMs = 3;
            // Slow playback chooses the LONGER of this duration and the longest frame at the
            // reference percentage. These are alternatives, not two independent hard limits.
            uint32_t slowFrameIntervalMs = 10'000;
            double slowSpeedPercent      = 1.0;

            bool IsValid() const;
            bool operator==(const Settings&) const = default;
        };

        enum class Limit
        {
            None,
            Minimum,
            Maximum
        };

        struct SpeedChange
        {
            bool changed = false;
            bool retimed = false;
            LLUtils::native_string_type message;
        };

        explicit SequencerPolicy(std::function<double()> elapsedSecondsProvider = {});

        // Retains the animation until the next Reset; pass no image when unloading. Normal speed
        // needs no frame scan under the default floor. Extrema are otherwise read once per image.
        void Reset(std::shared_ptr<IMCodec::Image> image = {});
        // Invalid settings retain the previous values; true means settings actually changed.
        bool SetSettings(const Settings& settings, Clock::time_point now = Clock::now());
        const Settings& GetSettings() const { return fSettings; }
        double GetSpeed() const { return fSpeed; }
        // Retimed requests need a timer update. A speed change that rounds to the same frame
        // duration keeps its deadline; blocked repeats only refresh the message.
        SpeedChange ChangeSpeed(double percent, Clock::time_point now = Clock::now());

        // Start timing the displayed frame. Retiming preserves its remaining fraction; a
        // 100 ms frame changed to 200 ms after 60 ms has 80 ms left, not a fresh 200 ms wait.
        uint32_t StartFrame(uint32_t delayMilliseconds, Clock::time_point now = Clock::now());
        // Zero means due (or inactive), never an interval to pass to the platform timer.
        // Supply monotonic timestamps when using an external/test clock.
        uint32_t RemainingFrameMs(Clock::time_point now = Clock::now()) const;

        static bool IsChangeSpeedCommand(const CommandManager::CommandArgs& args);
        static double ParseSpeedChangePercent(const CommandManager::CommandArgs& args);
        static double ApplySpeedChange(double currentSpeed, double percent, double effectiveSteps = 1.0);
        // Precision applies to the displayed percentage; prediction uses an unaccelerated +/-stepPercent.
        static LLUtils::native_string_type FormatSpeed(
            double speed, Limit limit = Limit::None, double frameIntervalMs = 0.0,
            UnitFormatter::Precision precision = UnitFormatter::Precision::predictive, double stepPercent = 1.0);
        static uint32_t NextFrame(uint32_t currentFrame, uint32_t frameCount);
        // speed must be finite and positive; minFrameIntervalMs must be in [1, MaxTimerIntervalMs]. The policy
        // enforces these at input/settings boundaries, leaving only bounded arithmetic per frame.
        static uint32_t FrameIntervalMs(uint32_t delayMilliseconds, double speed, uint32_t minFrameIntervalMs = 3);

      private:

        struct FrameDelays
        {
            uint32_t shortest;
            uint32_t longest;
        };
        struct SpeedLimits
        {
            double minimum;
            double maximum;
        };

        SpeedLimits GetSpeedLimits();
        bool RetimeFrame(Clock::time_point now);

        static constexpr uint32_t MinSourceDelayMs = 5;
        static constexpr double MotionBaseStep     = 1.0;
        // Scale accumulated acceleration down twentyfold; ChangeSpeed restores the baseline
        // so an isolated command still applies exactly one percentage step.
        static constexpr double MotionAcceleration    = 0.05;
        static constexpr double InitialMotionVelocity = MotionAcceleration * MotionBaseStep * MotionBaseStep;
        static constexpr double MotionDeceleration    = 5.0;

        Settings fSettings;
        std::shared_ptr<IMCodec::Image> fImage;
        std::optional<FrameDelays> fFrameDelays;
        AdaptiveMotion fMotion;
        double fSpeed          = 1.0;
        uint32_t fFrameDelayMs = 0;
        // Full duration at the current speed, distinct from the timer's remaining wait.
        // Zero marks inactive timing, including after an image reset.
        uint32_t fFrameIntervalMs = 0;
        Clock::time_point fFrameStart{};
        Clock::time_point fFrameDeadline{};
    };
}  // namespace OIV
