#include <catch2/catch_all.hpp>

#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVShared/AdaptiveMotion.h>
#include <Image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace
{
    IMCodec::ImageSharedPtr Animation(std::initializer_list<uint32_t> delays)
    {
        auto root      = std::make_shared<IMCodec::ImageItem>();
        root->itemType = IMCodec::ImageItemType::Container;
        auto image     = std::make_shared<IMCodec::Image>(root, IMCodec::ImageItemType::AnimationFrame);
        uint16_t index = 0;
        for (const auto delay : delays)
        {
            auto frame                             = std::make_shared<IMCodec::ImageItem>();
            frame->itemType                        = IMCodec::ImageItemType::AnimationFrame;
            frame->animationData.delayMilliseconds = delay;
            image->SetSubImage(index++, std::make_shared<IMCodec::Image>(frame, IMCodec::ImageItemType::Unknown));
        }
        return image;
    }
}  // namespace

TEST_CASE("Animation bounds use the shortest and longest normalized frames", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy;
    const auto image = Animation({200, 20, 75});
    policy.Reset(image);
    REQUIRE(policy.GetSpeed() == 1.0);
    auto change = policy.ChangeSpeed(1e100);
    REQUIRE(change.changed);
    REQUIRE(policy.GetSpeed() == Catch::Approx(20.0 / 3.0));
    REQUIRE(change.message == LLUTILS_TEXT("<textcolor=#ff8930>Animation speed<textcolor=#7672ff> (666.67%)\n"
                                           "<textcolor=#ff8930>Maximum animation speed reached<textcolor=#7672ff> "
                                           "(minimum frame time 3 ms)"));
    change = policy.ChangeSpeed(1.0);
    REQUIRE_FALSE(change.changed);
    REQUIRE(change.message.find(LLUTILS_TEXT("Maximum animation speed reached")) != LLUtils::native_string_type::npos);
    change = policy.ChangeSpeed(-99.9999);
    REQUIRE(change.changed);
    REQUIRE(policy.GetSpeed() == 1.0);  // Crossing normal speed stops there before the slow limit.
    change = policy.ChangeSpeed(-99.9999);
    REQUIRE(change.changed);
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.01));
    REQUIRE(change.message == LLUTILS_TEXT("<textcolor=#ff8930>Animation speed<textcolor=#7672ff> (1.00%)\n"
                                           "<textcolor=#ff8930>Minimum animation speed reached<textcolor=#7672ff> "
                                           "(maximum frame time 20.00 s)"));
    REQUIRE_FALSE(policy.ChangeSpeed(-1.0).changed);
    change = policy.ChangeSpeed(1.0);
    REQUIRE(change.changed);
    REQUIRE(change.message == OIV::SequencerPolicy::FormatSpeed(policy.GetSpeed()));
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.0101));
}

TEST_CASE("Animation slow limit chooses the longer frame duration", "[AppCore][Sequencer]")
{
    const auto [delay, minimumSpeed, interval] = GENERATE(table<uint32_t, double, uint32_t>({{20, 0.002, 10000},
                                                                                             {75, 0.0075, 10000},
                                                                                             {100, 0.01, 10000},
                                                                                             {200, 0.01, 20000},
                                                                                             {0, 0.0005, 10000},
                                                                                             {2, 0.0005, 10000},
                                                                                             {5, 0.0005, 10000}}));
    OIV::SequencerPolicy policy;
    policy.Reset(Animation({delay}));
    policy.ChangeSpeed(-99.9999);
    REQUIRE(policy.GetSpeed() == Catch::Approx(minimumSpeed));
    REQUIRE(OIV::SequencerPolicy::FrameIntervalMs(delay, policy.GetSpeed()) == interval);
    policy.ChangeSpeed(1e100);
    REQUIRE(policy.GetSpeed() == 1.0);
    policy.ChangeSpeed(1e100);
    REQUIRE(policy.GetSpeed() == Catch::Approx(std::max(5u, delay) / 3.0));
    REQUIRE(OIV::SequencerPolicy::FrameIntervalMs(delay, policy.GetSpeed()) == 3);
}

TEST_CASE("Animation input accelerates repeats and restores fine control", "[AppCore][Sequencer]")
{
    double elapsed = 0.0;
    OIV::SequencerPolicy policy([&] { return elapsed; });
    policy.Reset(Animation({1000}));
    policy.ChangeSpeed(1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(1.01));
    const double first = policy.GetSpeed();
    policy.ChangeSpeed(1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(first * std::pow(1.01, 1.15)));
    policy.ChangeSpeed(-1.0);
    REQUIRE(policy.GetSpeed() == 1.0);  // Reversing toward 100% uses the same snap as zoom.
    const double reversed = policy.GetSpeed();
    policy.ChangeSpeed(-1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(reversed * std::pow(0.99, 1.15)));
    elapsed                  = 2.0;
    const double beforePause = policy.GetSpeed();
    policy.ChangeSpeed(-1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(beforePause * 0.99));

    policy.Reset(Animation({6}));
    policy.ChangeSpeed(100.0);
    REQUIRE(policy.GetSpeed() == 2.0);
    REQUIRE_FALSE(policy.ChangeSpeed(1.0).changed);
    policy.ChangeSpeed(-1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(1.98));
}

TEST_CASE("Animation repeated input keeps a gentle acceleration above the one-percent baseline", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy([] { return 0.0; });
    policy.Reset(Animation({1000}));
    for (int i = 0; i < 10; ++i)
        policy.ChangeSpeed(1.0);
    // Ten instantaneous presses previously multiplied speed about 46 times. The reduced gain
    // adds about 33%, while still moving faster than ten isolated one-percent presses.
    REQUIRE(policy.GetSpeed() > std::pow(1.01, 10.0));
    REQUIRE(policy.GetSpeed() < 1.35);
}

TEST_CASE("Animation snaps approaching normal speed and can immediately leave it", "[AppCore][Sequencer]")
{
    const auto [current, percent, expected] = GENERATE(
        table<double, double, double>({{0.96, 2.0, 1.0},
                                       {1.04, -2.0, 1.0},
                                       {0.5, 200.0, 1.0},
                                       {2.0, -75.0, 1.0},  // Crossing beyond the whole snap range.
                                       {0.98, -1.0, 0.9702},
                                       {1.02, 1.0, 1.0302},  // Moving away must not snap.
                                       {1.0, 1.0, 1.01},
                                       {1.0, -1.0, 0.99},
                                       {0.98, 0.0, 0.98},
                                       {1.02, 0.0, 1.02},
                                       {0.9, 1.0, 0.909},
                                       {1.1, -1.0, 1.089}}));
    REQUIRE(OIV::SequencerPolicy::ApplySpeedChange(current, percent) == Catch::Approx(expected));

    OIV::SequencerPolicy policy([] { return 0.0; });
    policy.Reset(Animation({75}));
    policy.ChangeSpeed(-10.0);
    const auto change = policy.ChangeSpeed(20.0);
    REQUIRE(policy.GetSpeed() == 1.0);
    REQUIRE(change.message == OIV::SequencerPolicy::FormatSpeed(1.0));
    policy.ChangeSpeed(1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(std::pow(1.01, 1.15)));
}

TEST_CASE("Animation frame limits still apply when the normal-speed snap is outside them", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy;
    auto settings               = policy.GetSettings();
    settings.minFrameIntervalMs = 100;
    REQUIRE(policy.SetSettings(settings));
    policy.Reset(Animation({50}));
    REQUIRE(policy.GetSpeed() == 0.5);
    const auto change = policy.ChangeSpeed(200.0);
    REQUIRE_FALSE(change.changed);
    REQUIRE(policy.GetSpeed() == 0.5);
    REQUIRE(change.message.find(LLUTILS_TEXT("Maximum animation speed reached")) != LLUtils::native_string_type::npos);
}

TEST_CASE("Image and settings changes reset motion while extrema remain cached per image", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy([] { return 0.0; });
    auto image = Animation({100, 20});
    policy.Reset(image);
    policy.ChangeSpeed(1.0);
    policy.ChangeSpeed(1.0);
    image->GetSubImage(1)->GetImageItem()->animationData.delayMilliseconds = 1;
    auto settings                                                          = policy.GetSettings();
    settings.minFrameIntervalMs                                            = 2;
    REQUIRE(policy.SetSettings(settings));
    const double previous = policy.GetSpeed();
    policy.ChangeSpeed(1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(previous * 1.01));
    policy.ChangeSpeed(1e100);
    REQUIRE(policy.GetSpeed() == 10.0);  // Cached 20 ms shortest frame, with the new 2 ms floor.
    policy.Reset(image);
    REQUIRE(policy.GetSpeed() == 1.0);
    policy.ChangeSpeed(1.0);
    REQUIRE(policy.GetSpeed() == Catch::Approx(1.01));
    policy.ChangeSpeed(1e100);
    REQUIRE(policy.GetSpeed() == 2.5);  // New image lifetime re-reads and normalizes the 1 ms frame.
    policy.Reset();
    REQUIRE(policy.GetSpeed() == 1.0);
    REQUIRE(policy.ChangeSpeed(1.0).message.empty());
    policy.Reset(Animation({}));
    REQUIRE(policy.ChangeSpeed(-1.0).message.empty());
}

TEST_CASE("Animation custom settings clamp active speed and prioritize the playback floor", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy;
    policy.Reset(Animation({5, 200}));
    auto settings                = policy.GetSettings();
    settings.slowFrameIntervalMs = 20000;
    settings.slowSpeedPercent    = 2.0;
    REQUIRE(policy.SetSettings(settings));
    policy.ChangeSpeed(-99.9999);
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.01));
    settings.slowFrameIntervalMs = 1000;
    REQUIRE(policy.SetSettings(settings));
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.02));
    settings.minFrameIntervalMs = 10000;
    REQUIRE(policy.SetSettings(settings));
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.0005));
    REQUIRE_FALSE(policy.ChangeSpeed(-1).changed);
    REQUIRE_FALSE(policy.ChangeSpeed(1).changed);
    policy.Reset(Animation({5, 200}));
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.0005));
}

TEST_CASE("Invalid animation settings and commands retain valid playback", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy;
    policy.Reset(Animation({75}));
    const auto defaults = policy.GetSettings();
    for (const double value : {0.0, -1.0, 101.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min()})
    {
        auto settings             = defaults;
        settings.slowSpeedPercent = value;
        REQUIRE_FALSE(policy.SetSettings(settings));
        REQUIRE(policy.GetSettings() == defaults);
    }
    auto settings               = defaults;
    settings.minFrameIntervalMs = 0;
    REQUIRE_FALSE(policy.SetSettings(settings));
    settings                     = defaults;
    settings.slowFrameIntervalMs = 0;
    REQUIRE_FALSE(policy.SetSettings(settings));
    for (const auto* amount : {"", "bad", "1tail", "nan", "inf", "-100", "-101", "1e999"})
    {
        CAPTURE(amount);
        const auto args = OIV::CommandManager::CommandArgs::FromString(std::string("amount=") + amount);
        REQUIRE(OIV::SequencerPolicy::ParseSpeedChangePercent(args) == 0.0);
    }
    for (const double amount :
         {0.0, -100.0, -101.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        const auto change = policy.ChangeSpeed(amount);
        REQUIRE_FALSE(change.changed);
        REQUIRE(change.message.empty());
        REQUIRE(policy.GetSpeed() == 1.0);
    }
}

TEST_CASE("Animation settings parse whole milliseconds and percentages without truncation", "[AppCore][Sequencer]")
{
    using Action = OIV::AppSettingsPolicy::ActionType;
    auto parse   = [](const auto* key, const auto* value) { return OIV::AppSettingsPolicy::ParseAction(key, value); };
    REQUIRE(parse(LLUTILS_TEXT("animation/minframeintervalms"), LLUTILS_TEXT("3.0")).integralValue == 3);
    REQUIRE(parse(LLUTILS_TEXT("animation/minframeintervalms"), LLUTILS_TEXT("3")).type ==
            Action::AnimationMinFrameInterval);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("10000")).type ==
            Action::AnimationSlowFrameInterval);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowspeedpercent"), LLUTILS_TEXT("0.0004")).floatValue ==
            Catch::Approx(0.0004));
    for (const auto* value : {LLUTILS_TEXT("0"), LLUTILS_TEXT("-1"), LLUTILS_TEXT("nan"), LLUTILS_TEXT("inf"),
                              LLUTILS_TEXT("bad"), LLUTILS_TEXT("3tail"), LLUTILS_TEXT("1e999")})
    {
        REQUIRE(parse(LLUTILS_TEXT("animation/minframeintervalms"), value).type == Action::None);
        REQUIRE(parse(LLUTILS_TEXT("animation/slowspeedpercent"), value).type == Action::None);
    }
    REQUIRE(parse(LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("3.5")).type == Action::None);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("4294967296")).type == Action::None);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowspeedpercent"), LLUTILS_TEXT("101")).type == Action::None);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("4294967295")).type == Action::None);
    REQUIRE(parse(LLUTILS_TEXT("animation/minframeintervalms"), LLUTILS_TEXT("2147483648")).type == Action::None);
    REQUIRE(parse(LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("2147483647")).integralValue ==
            OIV::SequencerPolicy::MaxTimerIntervalMs);
}

TEST_CASE("Animation intervals are bounded before integer conversion", "[AppCore][Sequencer]")
{
    using Policy = OIV::SequencerPolicy;
    REQUIRE(Policy::FrameIntervalMs(20, 1e100) == 3);
    REQUIRE(Policy::FrameIntervalMs(2, 1.0) == 5);
    REQUIRE(Policy::FrameIntervalMs(20, 2.0) == 10);
    REQUIRE(Policy::FrameIntervalMs(20, std::nextafter(20.0 / 3.0, 100.0)) == 3);
    REQUIRE(Policy::FrameIntervalMs(20, 10.0, 7) == 7);
    REQUIRE(Policy::FrameIntervalMs(UINT32_MAX, 0.01) == Policy::MaxTimerIntervalMs);
    REQUIRE(Policy::FrameIntervalMs(UINT32_MAX, std::numeric_limits<double>::min()) == Policy::MaxTimerIntervalMs);
}

TEST_CASE("Animation percentages add only the precision needed to display nonzero speed", "[AppCore][Sequencer]")
{
    const auto [speed, expected] = GENERATE(
        table<double, const LLUtils::native_char_type*>({{1.25, LLUTILS_TEXT("125.00%")},
                                                         {0.0, LLUTILS_TEXT("0.00%")},
                                                         {0.0004, LLUTILS_TEXT("0.04%")},
                                                         {0.000004, LLUTILS_TEXT("0.0004%")},
                                                         {0.00000456, LLUTILS_TEXT("0.0005%")},
                                                         {0.000009, LLUTILS_TEXT("0.001%")},
                                                         {std::nextafter(0.00005, 0.0), LLUTILS_TEXT("0.005%")},
                                                         {std::nextafter(0.00005, 1.0), LLUTILS_TEXT("0.01%")},
                                                         {1e-12, LLUTILS_TEXT("0.0000000001%")}}));
    for (auto limit : {OIV::SequencerPolicy::Limit::None, OIV::SequencerPolicy::Limit::Minimum,
                       OIV::SequencerPolicy::Limit::Maximum})
    {
        const auto message   = OIV::SequencerPolicy::FormatSpeed(speed, limit, 3,
                                                                 OIV::UnitFormatter::PrecisionMode::preserve_nonzero);
        const auto firstLine = message.substr(0, message.find(LLUTILS_TEXT('\n')));
        REQUIRE(firstLine == OIV::SequencerPolicy::FormatSpeed(speed, OIV::SequencerPolicy::Limit::None, 0,
                                                               OIV::UnitFormatter::PrecisionMode::preserve_nonzero));
        REQUIRE((message.find(LLUTILS_TEXT('\n')) != LLUtils::native_string_type::npos) ==
                (limit != OIV::SequencerPolicy::Limit::None));
        REQUIRE(message.find(LLUtils::native_string_type(LLUTILS_TEXT("(")) + expected) !=
                LLUtils::native_string_type::npos);
    }
    OIV::SequencerPolicy policy;
    auto settings             = policy.GetSettings();
    settings.slowSpeedPercent = 0.0004;
    REQUIRE(policy.SetSettings(settings));
    policy.Reset(Animation({75}));
    const auto change = policy.ChangeSpeed(-99.9999);
    REQUIRE(change.message.find(LLUTILS_TEXT("(0.0004%")) != LLUtils::native_string_type::npos);
}

TEST_CASE("AdaptiveMotion reset begins a new fine interaction", "[AppCore][Shared][Sequencer]")
{
    OIV::AdaptiveMotion motion(1.0, 1.0, 5.0, [] { return 0.0; });
    REQUIRE(motion.Add(1) == 1.0);
    REQUIRE(motion.Add(1) == 4.0);
    motion.Reset();
    REQUIRE(motion.Add(1) == 1.0);
}

TEST_CASE("Animation reload stages all limits before clamping playback", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.0; });
    policy.Reset(Animation({75}));
    policy.ChangeSpeed(-99.9999, start);
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.0075));
    REQUIRE(policy.StartFrame(75, start) == 10000);

    auto staged = policy.GetSettings();
    const std::array edits{
        std::pair{LLUTILS_TEXT("animation/slowframeintervalms"), LLUTILS_TEXT("5000")},
        std::pair{LLUTILS_TEXT("animation/slowspeedpercent"), LLUTILS_TEXT("0.5")},
    };
    for (const auto& [key, value] : edits)
    {
        REQUIRE(OIV::AppSettingsPolicy::StageAnimationSetting(OIV::AppSettingsPolicy::ParseAction(key, value), staged));
        REQUIRE(policy.GetSpeed() == Catch::Approx(0.0075));
    }
    REQUIRE(policy.SetSettings(staged, start + 6s));
    REQUIRE(policy.GetSpeed() == Catch::Approx(0.0075));
    REQUIRE(policy.RemainingFrameMs(start + 6s) == 4000);
}

TEST_CASE("Invalid staged animation entries preserve other valid edits", "[AppCore][Sequencer]")
{
    OIV::SequencerPolicy policy;
    auto staged         = policy.GetSettings();
    const auto duration = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("animation/slowframeintervalms"),
                                                              LLUTILS_TEXT("5000"));
    REQUIRE(OIV::AppSettingsPolicy::StageAnimationSetting(duration, staged));
    // The parsed number is positive but cannot produce a representable reference duration.
    const auto tiny = OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("animation/slowspeedpercent"),
                                                          LLUTILS_TEXT("1e-300"));
    REQUIRE(OIV::AppSettingsPolicy::StageAnimationSetting(tiny, staged));
    REQUIRE(staged.slowSpeedPercent == 1.0);
    REQUIRE(staged.slowFrameIntervalMs == 5000);
    REQUIRE_FALSE(OIV::AppSettingsPolicy::StageAnimationSetting(
        OIV::AppSettingsPolicy::ParseAction(LLUTILS_TEXT("viewsettings/maxzoom"), LLUTILS_TEXT("50")), staged));
    REQUIRE(policy.SetSettings(staged));
    REQUIRE(policy.GetSettings().slowFrameIntervalMs == 5000);
}

TEST_CASE("Animation slowdown preserves the remaining frame fraction", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.0; });
    policy.Reset(Animation({100}));
    REQUIRE(policy.StartFrame(100, start) == 100);
    const auto change = policy.ChangeSpeed(-50.0, start + 60ms);
    REQUIRE(change.changed);
    REQUIRE(change.retimed);
    REQUIRE(policy.RemainingFrameMs(start + 60ms) == 80);
    REQUIRE(policy.RemainingFrameMs(start + 100ms) == 40);  // An old queued tick cannot finish the frame early.
    REQUIRE(policy.RemainingFrameMs(start + 139ms) == 1);
    REQUIRE(policy.RemainingFrameMs(start + 140ms) == 0);
    REQUIRE(policy.StartFrame(100, start + 140ms) == 200);
    REQUIRE(policy.RemainingFrameMs(start + 140ms) == 200);
}

TEST_CASE("Animation speedup preserves progress and due frames stay due", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.0; });
    policy.Reset(Animation({100}));
    policy.ChangeSpeed(-50.0, start);
    REQUIRE(policy.StartFrame(100, start) == 200);
    REQUIRE(policy.ChangeSpeed(100.0, start + 60ms).retimed);
    REQUIRE(policy.GetSpeed() == 1.0);
    REQUIRE(policy.RemainingFrameMs(start + 60ms) == 70);
    REQUIRE(policy.RemainingFrameMs(start + 130ms) == 0);
    REQUIRE_FALSE(policy.ChangeSpeed(-50.0, start + 140ms).retimed);
    REQUIRE(policy.RemainingFrameMs(start + 140ms) == 0);
    REQUIRE_FALSE(policy.ChangeSpeed(-50.0, start + 150ms).retimed);
    REQUIRE(policy.RemainingFrameMs(start + 150ms) == 0);
}

TEST_CASE("Repeated animation adjustments keep frames advancing", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.02; });
    policy.Reset(Animation({75}));
    policy.StartFrame(75, start);
    int completed = 0;
    for (int i = 1; i <= 30; ++i)
    {
        const auto now = start + i * 20ms;
        policy.ChangeSpeed(-1.0, now);
        if (policy.RemainingFrameMs(now) == 0)
        {
            ++completed;
            policy.StartFrame(75, now);
        }
    }
    REQUIRE(completed >= 3);  // Restarting the full wait for each command completes zero frames.
}

TEST_CASE("Unchanged animation durations and blocked requests do not restart waits", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.0; });
    policy.Reset(Animation({75}));
    policy.StartFrame(75, start);
    const auto rounded = policy.ChangeSpeed(-1.0, start + 20ms);
    REQUIRE(rounded.changed);
    REQUIRE_FALSE(rounded.retimed);
    REQUIRE(policy.RemainingFrameMs(start + 20ms) == 55);
    policy.ChangeSpeed(1.0, start + 30ms);  // Snap to 100%, with the same integer frame duration.
    REQUIRE(policy.RemainingFrameMs(start + 30ms) == 45);

    policy.Reset(Animation({75}));
    policy.ChangeSpeed(-99.9999, start);
    policy.StartFrame(75, start);
    const auto blocked = policy.ChangeSpeed(-1.0, start + 6s);
    REQUIRE_FALSE(blocked.changed);
    REQUIRE_FALSE(blocked.retimed);
    REQUIRE(policy.RemainingFrameMs(start + 6s) == 4000);
}

TEST_CASE("Animation settings preserve progress while enforcing a new frame floor", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy([] { return 0.0; });
    policy.Reset(Animation({100}));
    policy.StartFrame(100, start);
    auto settings               = policy.GetSettings();
    settings.minFrameIntervalMs = 200;
    REQUIRE(policy.SetSettings(settings, start + 60ms));
    REQUIRE(policy.GetSpeed() == 0.5);
    REQUIRE(policy.RemainingFrameMs(start + 60ms) == 140);  // The new hard floor overrides a 140 ms total.
    REQUIRE_FALSE(policy.SetSettings(settings, start + 100ms));
    REQUIRE(policy.RemainingFrameMs(start + 100ms) == 100);
    REQUIRE(policy.RemainingFrameMs(start + 200ms) == 0);
}

TEST_CASE("Animation reset clears timing and finite timer bounds exclude reserved values", "[AppCore][Sequencer]")
{
    using namespace std::chrono_literals;
    using Policy     = OIV::SequencerPolicy;
    const auto start = Policy::Clock::time_point{};
    Policy policy;
    for (const auto invalid : {Policy::MaxTimerIntervalMs + 1, UINT32_MAX})
    {
        auto settings               = policy.GetSettings();
        settings.minFrameIntervalMs = invalid;
        REQUIRE_FALSE(policy.SetSettings(settings, start));
        settings                     = policy.GetSettings();
        settings.slowFrameIntervalMs = invalid;
        REQUIRE_FALSE(policy.SetSettings(settings, start));
    }
    policy.Reset(Animation({UINT32_MAX}));
    REQUIRE(policy.StartFrame(UINT32_MAX, start) == Policy::MaxTimerIntervalMs);
    REQUIRE(policy.RemainingFrameMs(start) < UINT32_MAX);
    REQUIRE(policy.RemainingFrameMs(start + 1ms) == Policy::MaxTimerIntervalMs - 1);
    REQUIRE(policy.RemainingFrameMs(start + std::chrono::milliseconds(Policy::MaxTimerIntervalMs)) == 0);
    policy.Reset(Animation({75}));
    REQUIRE(policy.RemainingFrameMs(start) == 0);
    REQUIRE(policy.StartFrame(75, start) == 75);
    policy.Reset();
    REQUIRE(policy.RemainingFrameMs(start) == 0);
}

TEST_CASE("Step-aware animation precision follows the fixed command step", "[AppCore][Sequencer]")
{
    const auto [speed, step, expected] = GENERATE(table<double, double, const LLUtils::native_char_type*>({
        {1.0, 1.0, LLUTILS_TEXT("(100.00%)")},
        {0.01, 1.0, LLUTILS_TEXT("(1.00%)")},
        {0.0001, 1.0, LLUTILS_TEXT("(0.0100%)")},
        {0.00000456, 1.0, LLUTILS_TEXT("(0.000456%)")},
        {0.01, 0.1, LLUTILS_TEXT("(1.000%)")},
        {0.01, -0.1, LLUTILS_TEXT("(1.000%)")},
    }));
    for (auto limit : {OIV::SequencerPolicy::Limit::None, OIV::SequencerPolicy::Limit::Minimum,
                       OIV::SequencerPolicy::Limit::Maximum})
    {
        const auto message = OIV::SequencerPolicy::FormatSpeed(speed, limit, 3,
                                                               OIV::UnitFormatter::PrecisionMode::step_aware, step);
        REQUIRE(message.find(expected) != LLUtils::native_string_type::npos);
    }

    OIV::SequencerPolicy policy([] { return 0.0; });
    policy.Reset(Animation({75}));
    policy.ChangeSpeed(-99.0);
    const auto down = policy.ChangeSpeed(-1.0);
    REQUIRE(down.message == OIV::SequencerPolicy::FormatSpeed(policy.GetSpeed()));
    const auto up = policy.ChangeSpeed(0.1);
    REQUIRE(up.message == OIV::SequencerPolicy::FormatSpeed(policy.GetSpeed(), OIV::SequencerPolicy::Limit::None, 0,
                                                            OIV::UnitFormatter::PrecisionMode::step_aware, 0.1));
}
