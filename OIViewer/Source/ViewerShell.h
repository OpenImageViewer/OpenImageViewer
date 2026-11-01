#pragma once
#include <OIVAppCore/CommandController.h>
#include <OIVAppCore/FileAssociationPolicy.h>
#include <LWS/WindowTypes.hpp>
#include <functional>
namespace OIV
{
    // Frontend callbacks are installed before initialization and outlive the Viewer. Empty desktop callbacks mean
    // the service does not exist in that frontend, rather than creating dormant process-wide resources in a plugin.
    struct ViewerShell
    {
        std::function<void()> created;
        std::function<void()> ready;
        std::function<void()> stop;
        std::function<void(bool)> close;
        std::function<void()> destroyed;
        std::function<void()> showSettings;
        std::function<void()> topMost;
        std::function<void(LLUtils::native_string_type)> openExternalFile;
        std::function<void(const CommandManager::CommandRequest&, CommandManager::CommandResult&)> fileAssociations;
        std::function<void(const FileAssociationNotice&)> associationNotice;
        std::function<void(uint32_t, uint32_t, uint32_t)> forwardKey;
        CommandController::RequestPredicate commandAllowed{};
    };
}  // namespace OIV
