#pragma once
#include <OIVAppCore/CommandManager.h>

namespace OIV
{
    // A hosted viewer owns its content and fullscreen shell, while the host owns application lifetime
    // and desktop integration. Apply this policy at command dispatch, including configured key bindings.
    inline bool IsHostedCommandAllowed(const CommandManager::CommandRequest& request)
    {
        const auto& command = request.commandName;
        const auto type     = request.args.GetArgValue("type");
        return command != "cmd_file_associations" && command != "cmd_set_window_size" &&
               !(command == "cmd_shell" && request.args.GetArgValue("cmd") == "newWindow") &&
               !(command == "cmd_view_state" && type == "toggleBorders") &&
               !(command == "cmd_toggle_keybindings" && type == "settings");
    }
}  // namespace OIV
