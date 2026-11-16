#include "ViewerMenu.h"
#include <algorithm>

namespace OIV
{
    std::vector<LWSUI::MenuBarItem> BuildViewerMenu(const ViewerMenuState& s,
                                                    const ConfigurationLoader::CommandGroupList& commands,
                                                    const ConfigurationLoader::KeyBindingList& bindings,
                                                    std::function<void(std::string)> execute,
                                                    CommandController::RequestPredicate allowed)
    {
        // Resolve through the same configured groups as keyboard dispatch, including user remappings.
        auto item =
            [&](std::string label, std::string group, bool enabled = true, std::optional<bool> checked = std::nullopt)
        {
            LWSUI::MenuItem result;
            result.label       = std::move(label);
            result.checked     = checked;
            const auto command = std::ranges::find(commands, group, &ConfigurationLoader::CommandGroup::commandGroupID);
            result.enabled     = enabled && command != commands.end();
            if (result.enabled && allowed)
                result.enabled = allowed({command->commandDisplayName, command->commandName,
                                          CommandManager::CommandArgs::FromString(command->arguments)});
            std::vector<std::string> shortcuts;
            for (const auto& binding : bindings)
                if (binding.GroupID == group &&
                    std::ranges::find(shortcuts, binding.KeyCombinationName) == shortcuts.end())
                    shortcuts.push_back(binding.KeyCombinationName);
            for (const auto& shortcut : shortcuts)
            {
                if (!result.shortcut.empty())
                    result.shortcut += ", ";
                result.shortcut += shortcut;
            }
            result.action = [execute, group = std::move(group)] { execute(group); };
            return result;
        };
        auto submenu = [](std::string label, std::vector<LWSUI::MenuItem> items)
        {
            LWSUI::MenuItem result;
            result.label   = std::move(label);
            result.enabled = std::ranges::any_of(items, [](const auto& i) { return !i.separator && i.enabled; });
            result.submenu = std::move(items);
            return result;
        };
        const auto separator = LWSUI::MenuItem::Separator();
        return {
            {"&File",
             {item("&Open...", "OpenFile"), item("Save Image &As...", "SaveFile", s.image), separator,
              item("Open Containing &Folder", "OpenContainingFolder", s.file),
              item("Open Image in New &Window", "OpenImageInNewWindow", s.file), separator,
              item("Move to Recycle &Bin", "MoveFileToRecycleBin", s.file), separator,
              item("Minimize to &Tray", "CloseToTray", s.canTray), item("E&xit", "CloseApplication")}},
            {"&Edit",
             {item("Cu&t Selection", "Cut", s.selection),
              item("&Copy Image/Selection", "CopySelectedAreaToClipboard", s.image),
              item("&Paste", "PasteImageFromClipboard"), item("Copy File &Path", "CopyFileNameToClipboard", s.file),
              separator, item("Select &All", "SelectEntireImage", s.image), separator,
              item("&Settings...", "ShowSettings", s.canSettings)}},
            {"&Image",
             {item("&Resize...", "ShowResizeForm", s.image && s.canResize),
              item("&Crop to Selection", "CropToSelectedArea", s.selection), separator,
              item("Rotate &Left", "RotateCounterClockwise", s.image),
              item("Rotate &Right", "RotateClockwise", s.image), item("Flip &Horizontal", "HorizontalFlip", s.image),
              item("Flip &Vertical", "VerticalFlip", s.image), separator,
              submenu("Color &Adjustments", {item("Increase Gamma", "IncreaseGamma", s.image),
                                             item("Decrease Gamma", "DecreaseGamma", s.image),
                                             item("Increase Exposure", "IncreaseExposure", s.image),
                                             item("Decrease Exposure", "Decrease Exposure", s.image),
                                             item("Increase Offset", "IncreaseOffset", s.image),
                                             item("Decrease Offset", "DecreaseOffset", s.image),
                                             item("Increase Saturation", "IncreaseSaturation", s.image),
                                             item("Decrease Saturation", "DecreaseSaturation", s.image), separator,
                                             item("Reset/Restore Color Adjustments", "ToggleColorCorrection", s.image)}),
              separator, item("Image &Information", "ShowImageInfo", s.image)}},
            {"&View",
             {item("Zoom &In", "ZoomIn", s.image), item("Zoom &Out", "ZoomOut", s.image),
              item("&Actual Size", "ViewOriginalSize", s.image), item("&Fit to Window", "FitToWindow", s.image),
              item("&Center", "Center", s.image), separator, item("&Fullscreen", "ToggleFullScren", true, s.fullscreen),
              item("Fullscreen Across &Monitors", "ToggleMultiFullScreen", s.canMultiFullscreen, s.multiFullscreen),
              separator,
              submenu("&Interface", {item("&Borders", "ToggleWindowBorders", true, s.interfacePreferences.borders),
                                     item("&Menu Bar", "ToggleMenuBar", true, s.interfacePreferences.menuBar),
                                     item("&Caption", "ToggleCaption", true, s.interfacePreferences.caption),
                                     item("&Status Bar", "ToggleStatusBar", true, s.interfacePreferences.statusBar),
                                     item("Sub-image &Panel", "ToggleSubImagePanel", s.subImages, s.showSubImages),
                                     separator, item("Restore Default Interface", "RestoreDefaultInterface")}),
              separator, item("&Grid", "ToggleGrid", s.image, s.grid),
              item("&Slideshow", "ToggleSlideShow", s.navigation, s.slideshow), separator,
              submenu("&Navigate", {item("Previous Image", "PreviousImageInFolder", s.navigation),
                                    item("Next Image", "NextImageInFolder", s.navigation),
                                    item("First Image", "FirstImageInFolder", s.navigation),
                                    item("Last Image", "LastImageInFolder", s.navigation)}),
              submenu("So&rt", {item("Name", "SortByName", s.navigation, s.sort == FileSorter::SortType::Name),
                                item("Date", "SortByDate", s.navigation, s.sort == FileSorter::SortType::Date),
                                item("Extension", "SortByExtension", s.navigation,
                                     s.sort == FileSorter::SortType::Extension)})}},
            {"&Help",
             {item("&Keyboard Shortcuts", "ToggleKeyBindings"), item("&System Information", "ShowSystemInfo"),
              separator, item("OIViewer &Website", "OpenWebsite"),
              item("&About OIViewer...", "ShowAbout", s.canAbout)}}};
    }
}  // namespace OIV
