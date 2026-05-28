#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/CommandManager.h>
#include <OIVAppCore/FolderFileList.h>

#include <LLUtils/Point.h>

#include <cstdint>
#include <string>

namespace OIV
{
    struct ZoomCommand
    {
        double amount   = 0.0;
        int32_t centerX = -1;
        int32_t centerY = -1;
    };

    enum class PanDirection
    {
        None,
        Up,
        Down,
        Left,
        Right
    };

    struct PanCommand
    {
        PanDirection direction = PanDirection::None;
        double amount          = 0.0;
    };

    enum class PlacementAction
    {
        None,
        OriginalSize,
        FitToScreen,
        Center
    };

    struct NavigationCommand
    {
        FolderFileList::index_type amount = 0;
        bool subImage                     = false;
    };

    enum class WindowSizeMode
    {
        None,
        Fullscreen,
        MultiFullscreen,
        Windowed
    };

    struct WindowWorkingArea
    {
        int32_t left   = 0;
        int32_t top    = 0;
        int32_t right  = 0;
        int32_t bottom = 0;

        int32_t Width() const;
        int32_t Height() const;
    };

    struct WindowSizeDecision
    {
        WindowSizeMode mode = WindowSizeMode::None;
        LLUtils::PointI32 size{};
        LLUtils::PointI32 position{};
    };

    class ViewCommandPolicy
    {
      public:

        static ZoomCommand ParseZoom(const CommandManager::CommandArgs& args);
        static LLUtils::native_string_type FormatZoomResult(double scale);

        static PanCommand ParsePan(const CommandManager::CommandArgs& args);
        static LLUtils::native_string_type FormatPanResult(const std::string& displayName, double amount);

        static PlacementAction ParsePlacement(const CommandManager::CommandArgs& args);
        static LLUtils::native_string_type FormatPlacementResult(const std::string& displayName);

        static NavigationCommand ParseNavigation(const CommandManager::CommandArgs& args);
        static FolderFileList::index_type NextSubImageIndex(FolderFileList::index_type selected,
                                                            FolderFileList::index_type amount,
                                                            FolderFileList::index_type count);

        static WindowSizeDecision DecideWindowSize(const CommandManager::CommandArgs& args,
                                                   LLUtils::PointI32 currentWindowSize,
                                                   LLUtils::PointI32 currentPosition, WindowWorkingArea workingArea);
    };
}  // namespace OIV
