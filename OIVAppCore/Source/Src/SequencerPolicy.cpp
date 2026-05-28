#include <LLUtils/StringDefs.h>
#include <OIVAppCore/SequencerPolicy.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace OIV
{
    bool SequencerPolicy::IsChangeSpeedCommand(const CommandManager::CommandArgs& args)
    {
        return args.GetArgValue("cmd") == "changespeed";
    }

    double SequencerPolicy::ParseSpeedChangePercent(const CommandManager::CommandArgs& args)
    {
        return std::stod(args.GetArgValue("amount"));
    }

    double SequencerPolicy::ApplySpeedChange(double currentSpeed, double percent)
    {
        return currentSpeed * (1 + percent / 100.0);
    }

    LLUtils::native_string_type SequencerPolicy::FormatSpeed(double speed)
    {
        LLUtils::native_stringstream stream;
        stream << "<textcolor=#ff8930>" << LLUTILS_TEXT("Animation speed") << LLUTILS_TEXT("<textcolor=#7672ff> (")
               << std::fixed << std::setprecision(2) << speed * 100 << "%)";
        return stream.str();
    }

    uint32_t SequencerPolicy::NextFrame(uint32_t currentFrame, uint32_t frameCount)
    {
        return frameCount == 0 ? 0 : (currentFrame + 1) % frameCount;
    }

    uint32_t SequencerPolicy::FrameIntervalMs(uint32_t delayMilliseconds, double speed)
    {
        constexpr uint32_t MinFrameDelay = 5u;
        return std::max(1u,
                        static_cast<uint32_t>(static_cast<double>(std::max(MinFrameDelay, delayMilliseconds)) / speed));
    }
}  // namespace OIV
