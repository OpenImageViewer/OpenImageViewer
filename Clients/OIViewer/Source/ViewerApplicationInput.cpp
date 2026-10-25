#include "ViewerApplication.h"

#include <OIVAppCore/ViewerPresentationPolicy.h>

#include <filesystem>

namespace OIV
{
    void ViewerApplication::ProcessTopMost()
    {
        if (fTopMostCounter <= 0)
            return;

        --fTopMostCounter;
        if (fTopMostCounter == 0)
        {
            fTimerTopMostRetention.SetInterval(0);
            std::ignore = fViewer.fWindow.GetWindow().SetAlwaysOnTop(false);
            fViewer.fMessageManager->RemoveGroup(static_cast<GroupID>(UserMessageGroups::WindowOnTop));
        }
        else
        {
            SetTopMostUserMesage();
        }
    }

    void ViewerApplication::SetTopMostUserMesage()
    {
        fViewer.SetUserMessage(ViewerPresentationPolicy::FormatTopMostMessage(fTopMostCounter),
                               static_cast<GroupID>(UserMessageGroups::WindowOnTop),
                               MessageFlags::Interchangeable | MessageFlags::ManualRemove);
    }
}  // namespace OIV
