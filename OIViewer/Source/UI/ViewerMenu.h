#pragma once
#include "InterfacePreferences.h"
#include <LWSUI/Menu.hpp>
#include <OIVAppCore/ConfigurationLoader.h>
#include <OIVAppCore/CommandController.h>
#include <OIVShared/FileSorter.h>

namespace OIV
{
    struct ViewerMenuState
    {
        InterfacePreferences interfacePreferences;
        bool image = false, selection = false, file = false, navigation = false;
        bool subImages = false, showSubImages = true;
        bool fullscreen = false, multiFullscreen = false, grid = false, slideshow = false;
        bool canMultiFullscreen = false, canTray = false, canResize = false, canSettings = false, canAbout = false;
        FileSorter::SortType sort = FileSorter::SortType::Name;
    };
    std::vector<LWSUI::MenuBarItem> BuildViewerMenu(const ViewerMenuState& state,
                                                    const ConfigurationLoader::CommandGroupList& commands,
                                                    const ConfigurationLoader::KeyBindingList& bindings,
                                                    std::function<void(std::string)> execute,
                                                    CommandController::RequestPredicate allowed = nullptr);
}  // namespace OIV
