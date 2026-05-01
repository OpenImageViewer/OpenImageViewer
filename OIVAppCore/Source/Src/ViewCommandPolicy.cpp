#include <OIVAppCore/ViewCommandPolicy.h>

#include <LLUtils/MathUtil.h>
#include <LLUtils/StringUtility.h>

#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <sstream>

namespace OIV
{
    int32_t WindowWorkingArea::Width() const
    {
        return right - left;
    }

    int32_t WindowWorkingArea::Height() const
    {
        return bottom - top;
    }

    ZoomCommand ViewCommandPolicy::ParseZoom(const CommandManager::CommandArgs& args)
    {
        const std::string cx = args.GetArgValue("cx");
        const std::string cy = args.GetArgValue("cy");

        return ZoomCommand{
            std::atof(args.GetArgValue("val").c_str()),
            cx.empty() ? -1 : std::atoi(cx.c_str()),
            cy.empty() ? -1 : std::atoi(cy.c_str())};
    }

    std::wstring ViewCommandPolicy::FormatZoomResult(double scale)
    {
        std::wstringstream stream;
        stream << "<textcolor=#ff8930>Zoom <textcolor=#7672ff>(" << std::fixed << std::setprecision(2)
               << scale * 100.0 << "%)";
        return stream.str();
    }

    PanCommand ViewCommandPolicy::ParsePan(const CommandManager::CommandArgs& args)
    {
        PanCommand command;
        const std::string direction = args.GetArgValue("direction");

        if (direction == "up")
            command.direction = PanDirection::Up;
        else if (direction == "down")
            command.direction = PanDirection::Down;
        else if (direction == "left")
            command.direction = PanDirection::Left;
        else if (direction == "right")
            command.direction = PanDirection::Right;

        command.amount = std::atof(args.GetArgValue("amount").c_str());
        return command;
    }

    std::wstring ViewCommandPolicy::FormatPanResult(const std::string& displayName, double amount)
    {
        std::wstringstream stream;
        stream << "<textcolor=#00ff00>" << LLUtils::StringUtility::ToWString(displayName)
               << "<textcolor=#7672ff> (" << amount << " pixels)";
        return stream.str();
    }

    PlacementAction ViewCommandPolicy::ParsePlacement(const CommandManager::CommandArgs& args)
    {
        const std::string command = args.GetArgValue("cmd");

        if (command == "originalSize")
            return PlacementAction::OriginalSize;
        if (command == "fitToScreen")
            return PlacementAction::FitToScreen;
        if (command == "center")
            return PlacementAction::Center;

        return PlacementAction::None;
    }

    std::wstring ViewCommandPolicy::FormatPlacementResult(const std::string& displayName)
    {
        return L"<textcolor=#00ff00>" + LLUtils::StringUtility::ToWString(displayName);
    }

    NavigationCommand ViewCommandPolicy::ParseNavigation(const CommandManager::CommandArgs& args)
    {
        const std::string amount = args.GetArgValue("amount");
        return NavigationCommand{
            amount == "start" ? FileList::IndexStart : amount == "end" ? FileList::IndexEnd : std::stoi(amount),
            args.GetArgValue("subimage") == "true"};
    }

    FileList::index_type ViewCommandPolicy::NextSubImageIndex(FileList::index_type selected,
                                                              FileList::index_type amount,
                                                              FileList::index_type count)
    {
        if (count <= 0)
            return 0;

        return LLUtils::Math::Modulu<FileList::index_type>(selected + amount, count);
    }

    WindowSizeDecision ViewCommandPolicy::DecideWindowSize(const CommandManager::CommandArgs& args,
                                                           LLUtils::PointI32 currentWindowSize,
                                                           LLUtils::PointI32 currentPosition,
                                                           WindowWorkingArea workingArea)
    {
        const std::string sizeType = args.GetArgValue("size_type");
        if (sizeType == "fullscreen")
            return {WindowSizeMode::Fullscreen};
        if (sizeType == "multifullscreen")
            return {WindowSizeMode::MultiFullscreen};
        if (sizeType != "absolute" && sizeType != "relative")
            return {};

        const double width = std::stod(args.GetArgValue("width"));
        const double height = std::stod(args.GetArgValue("height"));

        int32_t finalWidth = 0;
        int32_t finalHeight = 0;

        if (sizeType == "absolute")
        {
            finalWidth = static_cast<int32_t>(std::round(width));
            finalHeight = static_cast<int32_t>(std::round(height));
        }
        else
        {
            finalWidth = static_cast<int32_t>(std::round(width * workingArea.Width() / 100.0));
            finalHeight = static_cast<int32_t>(std::round(height * workingArea.Height() / 100.0));
        }

        finalWidth = std::min(workingArea.Width(), finalWidth);
        finalHeight = std::min(workingArea.Height(), finalHeight);

        const LLUtils::PointI32 windowNewSize{finalWidth, finalHeight};
        auto displacedPos = currentPosition - (windowNewSize - currentWindowSize) / 2;

        displacedPos.x = std::clamp<int32_t>(displacedPos.x, workingArea.left, workingArea.right - windowNewSize.x);
        displacedPos.y = std::clamp<int32_t>(displacedPos.y, workingArea.top, workingArea.bottom - windowNewSize.y);

        return {WindowSizeMode::Windowed, windowNewSize, displacedPos};
    }
}
