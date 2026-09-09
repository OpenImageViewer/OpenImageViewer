#include "ViewerApplication.h"

#include <OIVAppCore/ViewerPresentationPolicy.h>

#include <filesystem>

namespace OIV
{
    void ViewerApplication::SetTopMostUserMesage()
    {
        SetUserMessage(ViewerPresentationPolicy::FormatTopMostMessage(fTopMostCounter),
                       static_cast<GroupID>(UserMessageGroups::WindowOnTop),
                       MessageFlags::Interchangeable | MessageFlags::ManualRemove);
    }

    bool ViewerApplication::GetAppActive() const
    {
        return fIsActive;
    }

    void ViewerApplication::SetAppActive(bool active)
    {
        if (active == fIsActive)
            return;

        fIsActive = active;
        if (fIsActive && fFileReloadPolicy.HasPendingReloadFor(GetOpenedFileName()))
            PerformReloadFile(GetOpenedFileName());
        else
            UpdateTitle();
    }

    void ViewerApplication::ProcessTopMost()
    {
        if (fTopMostCounter <= 0)
            return;

        --fTopMostCounter;
        if (fTopMostCounter == 0)
        {
            fTimerTopMostRetention.SetInterval(0);
            std::ignore = fWindow.GetWindow().SetAlwaysOnTop(false);
            fMessageManager->RemoveGroup(static_cast<GroupID>(UserMessageGroups::WindowOnTop));
        }
        else
        {
            SetTopMostUserMesage();
        }
    }

    bool ViewerApplication::HandleFileDragDropEvent(const LWS::EventDragDropFile& eventDragDropFile)
    {
        const LLUtils::native_string_type normalizedPath =
            std::filesystem::path(eventDragDropFile.fileName).lexically_normal().native();
        if (!LoadFileOrFolder(normalizedPath,
                              IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
            return false;

        std::ignore = fWindow.GetWindow().RequestActivation();
        return true;
    }

    bool ViewerApplication::HandleMessages(const LWS::AnyEvent& eventData)
    {
        bool handled = true;
        if (std::holds_alternative<LWS::EventCloseRequested>(eventData))
        {
            // Suppress native destruction until the renderer and its images have been released.
            CloseApplication(false);
        }
        else if (const auto* dragDropEvent = std::get_if<LWS::EventDragDropFile>(&eventData))
            handled = HandleFileDragDropEvent(*dragDropEvent);
        else
            handled = HandleWinMessageEvent(eventData);
        return handled;
    }

    bool ViewerApplication::HandleClientWindowMessages(const LWS::AnyEvent& eventData)
    {
        return ClientWindwMessage(eventData) != 0;
    }
}  // namespace OIV
