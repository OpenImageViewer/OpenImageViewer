#include "ViewerApplication.h"
#include "FileAssociations.h"

#include <LWS/Win32/WindowExtensions.hpp>

namespace OIV
{
    namespace
    {
        LLUtils::LogCategory AssociationLog()
        {
            static const auto category = LLUtils::Logger::RegisterCategory("FileAssociations");
            return category;
        }

    }  // namespace

    void ViewerApplication::HandleFileAssociationNotice(const FileAssociationNotice& notice)
    {
        if (fViewer.fIsShuttingDown || fFileAssociations == nullptr ||
            (notice.requestId != 0 && notice.requestId != fAssociationRequestId))
            return;

        const HWND owner = *LWS::Win32::GetHwnd(fViewer.fWindow.GetWindow());
        switch (notice.kind)
        {
            case FileAssociationNotice::Kind::Prompt:
                ShowFileAssociationPrompt(notice.requestId);
                break;
            case FileAssociationNotice::Kind::OpenSettings:
            {
                fViewer.SetUserMessage(notice.message, 0, MessageFlags::Persistent);
                LL_LOG(AssociationLog(), LLUtils::LogLevel::Warning, "{}",
                       LLUtils::StringUtility::ToAString(notice.message));
                if (!Win32::OpenFileAssociationSettings(owner))
                    HandleFileAssociationNotice({FileAssociationNotice::Kind::ActionFailed,
                                                 L"Formats were registered, but Windows Settings could not be opened. "
                                                 L"Open Settings > Apps > Default apps and select Open Image Viewer."});
                break;
            }
            case FileAssociationNotice::Kind::Completed:
                fViewer.SetUserMessage(notice.message);
                break;
            case FileAssociationNotice::Kind::ActionFailed:
                fViewer.SetUserMessage(notice.message);
                [[fallthrough]];
            case FileAssociationNotice::Kind::CheckFailed:
                LL_LOG(AssociationLog(), LLUtils::LogLevel::Warning, "{}",
                       LLUtils::StringUtility::ToAString(notice.message));
                break;
        }
    }
}  // namespace OIV
