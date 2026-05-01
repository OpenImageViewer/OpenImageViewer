#include <OIVAppCore/ColorCorrectionCommandPolicy.h>

#include <LLUtils/StringUtility.h>

#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace OIV
{
    ColorCorrectionCommand ColorCorrectionCommandPolicy::Parse(const CommandManager::CommandArgs& args)
    {
        ColorCorrectionCommand command;
        command.channelName = args.GetArgValue("type");
        command.operation = args.GetArgValue("op");
        command.valueText = args.GetArgValue("val");
        command.value = std::atof(command.valueText.c_str());

        if (command.channelName == "gamma")
            command.channel = ColorCorrectionChannel::Gamma;
        else if (command.channelName == "exposure")
            command.channel = ColorCorrectionChannel::Exposure;
        else if (command.channelName == "offset")
            command.channel = ColorCorrectionChannel::Offset;
        else if (command.channelName == "saturation")
            command.channel = ColorCorrectionChannel::Saturation;
        else if (command.channelName == "contrast")
            command.channel = ColorCorrectionChannel::Contrast;

        return command;
    }

    double ColorCorrectionCommandPolicy::Apply(double currentValue, const std::string& operation, double value)
    {
        if (operation == "increase")
            return currentValue * (1 + value / 100.0);
        if (operation == "decrease")
            return currentValue * (1 / (1 + value / 100.0));
        if (operation == "add")
            return currentValue + value;
        if (operation == "subtract")
            return currentValue - value;

        return currentValue;
    }

    std::wstring ColorCorrectionCommandPolicy::FormatResult(const ColorCorrectionCommand& command, double newValue)
    {
        std::wstringstream stream;
        stream << L"<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(command.channelName)
               << L"<textcolor=#7672ff> " << LLUtils::StringUtility::ToWString(command.operation) << L" "
               << LLUtils::StringUtility::ToWString(command.valueText);

        if (command.operation == "increase" || command.operation == "decrease")
            stream << "%";

        stream << "<textcolor=#00ff00> (" << std::fixed << std::setprecision(0) << newValue * 100 << "%)";
        return stream.str();
    }
}
