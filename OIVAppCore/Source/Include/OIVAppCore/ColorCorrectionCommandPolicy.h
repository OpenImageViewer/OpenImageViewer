#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/CommandManager.h>

#include <string>

namespace OIV
{
    enum class ColorCorrectionChannel
    {
        None,
        Gamma,
        Exposure,
        Offset,
        Saturation,
        Contrast
    };

    struct ColorCorrectionCommand
    {
        ColorCorrectionChannel channel = ColorCorrectionChannel::None;
        std::string channelName;
        std::string operation;
        std::string valueText;
        double value = 0.0;

        bool IsValid() const { return channel != ColorCorrectionChannel::None; }
    };

    class ColorCorrectionCommandPolicy
    {
      public:

        static ColorCorrectionCommand Parse(const CommandManager::CommandArgs& args);
        static double Apply(double currentValue, const std::string& operation, double value);
        static LLUtils::native_string_type FormatResult(const ColorCorrectionCommand& command, double newValue);
    };
}  // namespace OIV
