#include "ViewerApplication.h"

namespace OIV
{
    void ViewerApplication::HandleFileAssociationNotice(const FileAssociationNotice& notice)
    {
        if (fViewer.fIsShuttingDown || !fFileAssociations || notice.requestId != fAssociationRequestId)
            return;
        if (notice.kind == FileAssociationNotice::Kind::Prompt)
            ShowFileAssociationPrompt(notice.requestId);
        else if (!notice.message.empty())
            fViewer.SetUserMessage(notice.message);
    }
}  // namespace OIV
