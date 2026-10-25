#include "ViewerApplication.h"
#include "ViewerApplicationPlatformState.h"

#include <LWS/Win32/WindowExtensions.hpp>

#include <CommCtrl.h>
#include <shellapi.h>

namespace OIV
{
    namespace
    {
        LLUtils::LogCategory AssociationLog()
        {
            static const auto category = LLUtils::Logger::RegisterCategory("FileAssociations");
            return category;
        }

        HRESULT CALLBACK TrackAssociationDialog(HWND window, UINT notification, WPARAM, LPARAM, LONG_PTR context)
        {
            auto& dialog = *reinterpret_cast<HWND*>(context);
            if (notification == TDN_CREATED)
                dialog = window;
            else if (notification == TDN_DESTROYED)
                dialog = nullptr;
            return S_OK;
        }
    }  // namespace

    void ViewerApplication::CloseFileAssociationDialog()
    {
        if (fNativeWindowState->associationDialog != nullptr)
            SendMessageW(fNativeWindowState->associationDialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
    }

    void ViewerApplication::HandleFileAssociationNotice(const FileAssociationNotice& notice)
    {
        if (fViewer.fIsShuttingDown || fFileAssociations == nullptr ||
            (notice.requestId != 0 && notice.requestId != fAssociationRequestId))
            return;

        const HWND owner = *LWS::Win32::GetHwnd(fViewer.fWindow.GetWindow());
        switch (notice.kind)
        {
            case FileAssociationNotice::Kind::Prompt:
            {
                constexpr int GlobalOptOutButton      = 100;
                constexpr TASKDIALOG_BUTTON buttons[] = {{IDYES, L"Associate"},
                                                         {IDCANCEL, L"Ask next time"},
                                                         {IDNO, L"Don't ask for this copy"},
                                                         {GlobalOptOutButton, L"Don't ask for any copy"}};
                const TASKDIALOGCONFIG config{
                    .cbSize             = sizeof(TASKDIALOGCONFIG),
                    .hwndParent         = owner,
                    .dwFlags            = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW,
                    .pszWindowTitle     = L"Open Image Viewer",
                    .pszMainInstruction = L"Make OIViewer the default for all supported image formats?",
                    .pszContent         = L"OIViewer is not the default for some supported image formats.\n\n"
                                          L"You can change associations later with Ctrl+Shift+A to add them "
                                          L"or Ctrl+Shift+R to remove them.\n\n"
                                          L"Ask next time shows this prompt on the next launch.",
                    .cButtons           = static_cast<UINT>(std::size(buttons)),
                    .pButtons           = buttons,
                    .nDefaultButton     = IDCANCEL,
                    .pfCallback         = TrackAssociationDialog,
                    .lpCallbackData     = reinterpret_cast<LONG_PTR>(&fNativeWindowState->associationDialog),
                };
                int button           = IDCANCEL;
                const HRESULT result = TaskDialogIndirect(&config, &button, nullptr, nullptr);
                // A task dialog runs nested dispatch. The owner may have shut down while it was open.
                if (!fViewer.fIsShuttingDown && fFileAssociations != nullptr &&
                    notice.requestId == fAssociationRequestId)
                {
                    const auto choice = SUCCEEDED(result) && button == IDYES ? FileAssociationChoice::Associate
                                        : SUCCEEDED(result) && button == GlobalOptOutButton
                                            ? FileAssociationChoice::SuppressForUser
                                        : SUCCEEDED(result) && button == IDNO ? FileAssociationChoice::SuppressForCopy
                                                                              : FileAssociationChoice::AskNextTime;
                    fFileAssociations->Respond(choice);
                    if (FAILED(result))
                        HandleFileAssociationNotice({FileAssociationNotice::Kind::ActionFailed,
                                                     L"Could not display the file association prompt."});
                }
                break;
            }
            case FileAssociationNotice::Kind::OpenSettings:
            {
                fViewer.SetUserMessage(notice.message, 0, MessageFlags::Persistent);
                LL_LOG(AssociationLog(), LLUtils::LogLevel::Warning, "{}",
                       LLUtils::StringUtility::ToAString(notice.message));
                auto launched = reinterpret_cast<INT_PTR>(
                    ShellExecuteW(owner, L"open", L"ms-settings:defaultapps?registeredAppUser=OIViewer", nullptr,
                                  nullptr, SW_SHOWNORMAL));
                if (launched <= 32)
                    launched = reinterpret_cast<INT_PTR>(
                        ShellExecuteW(owner, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL));
                if (launched <= 32)
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
