#include "Viewer.h"
#include "ViewerMouseInput.h"

#include <OIVAppCore/ViewerPresentationPolicy.h>

#include <filesystem>

namespace OIV
{

    bool Viewer::GetAppActive() const
    {
        return fIsActive;
    }

    void Viewer::SetAppActive(bool active)
    {
        if (active == fIsActive)
            return;

        fIsActive = active;
        if (fIsActive && fFileReloadPolicy.HasPendingReloadFor(GetOpenedFileName()))
            PerformReloadFile(GetOpenedFileName());
        else
            UpdateTitle();
    }

    bool Viewer::HandleFileDragDropEvent(const LWS::EventDragDropFile& eventDragDropFile)
    {
        const LLUtils::native_string_type normalizedPath =
            std::filesystem::path(eventDragDropFile.fileName).lexically_normal().native();
        if (!LoadFileOrFolder(normalizedPath,
                              IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
            return false;

        std::ignore = fWindow.GetWindow().RequestActivation();
        return true;
    }

    bool Viewer::HandleMessages(const LWS::AnyEvent& eventData)
    {
        bool handled = true;
        if (std::holds_alternative<LWS::EventMouseCaptureLost>(eventData))
            fMouseInput->CaptureLost();
        else if (std::holds_alternative<LWS::EventCloseRequested>(eventData))
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

    bool Viewer::HandleClientWindowMessages(const LWS::AnyEvent& eventData)
    {
        // Capture belongs to the native window that received the press, which can be the canvas or main window.
        if (std::holds_alternative<LWS::EventMouseCaptureLost>(eventData))
        {
            fMouseInput->CaptureLost();
            return true;
        }
        // Canvas geometry changes update the presentation identically on every backend.
        if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(eventData))
        {
            fRefreshOperation.Begin();
            UpdateWindowSize();
            fRefreshOperation.End();
            return true;
        }
        return ClientWindwMessage(eventData) != 0;
    }
}  // namespace OIV
