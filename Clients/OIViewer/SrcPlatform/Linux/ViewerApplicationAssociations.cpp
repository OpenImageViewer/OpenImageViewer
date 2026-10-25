#include "ViewerApplication.h"
#include "ViewerApplicationPlatformState.h"

namespace OIV
{
    void ViewerApplication::CloseFileAssociationDialog()
    {
        auto& state = *fNativeWindowState;
        if (state.associationDialogPump != nullptr)
            state.associationDialogPump->Enable(false);
        if (state.associationDialog != nullptr)
        {
            gtk_widget_destroy(state.associationDialog);
            state.associationDialog = nullptr;
            // LWS owns the outer loop, so GTK will not flush destruction requests after this callback.
            gdk_display_flush(gdk_display_get_default());
        }
    }

    void ViewerApplication::HandleFileAssociationNotice(const FileAssociationNotice& notice)
    {
        if (fViewer.fIsShuttingDown || fFileAssociations == nullptr || notice.requestId != fAssociationRequestId)
            return;
        if (notice.kind == FileAssociationNotice::Kind::Prompt)
        {
            if (gtk_init_check(nullptr, nullptr) == FALSE)
            {
                fFileAssociations->Respond(FileAssociationChoice::AskNextTime);
                return;
            }
            auto& state             = *fNativeWindowState;
            state.associationDialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                                             GTK_BUTTONS_NONE,
                                                             "Make OIViewer the default for supported image formats?");
            constexpr int SuppressForUserResponse = 1;
            gtk_dialog_add_buttons(GTK_DIALOG(state.associationDialog), "_Associate", GTK_RESPONSE_YES,
                                   "Ask _next time", GTK_RESPONSE_CANCEL, "Don't ask for _this copy", GTK_RESPONSE_NO,
                                   "Don't ask for an_y copy", SuppressForUserResponse, nullptr);
            gtk_window_set_title(GTK_WINDOW(state.associationDialog), "Open Image Viewer");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(state.associationDialog),
                                                     "You can change associations later with Ctrl+Shift+A to add them "
                                                     "or Ctrl+Shift+R to remove them. "
                                                     "Ask next time shows this prompt on the next launch.");
            gtk_dialog_set_default_response(GTK_DIALOG(state.associationDialog), GTK_RESPONSE_CANCEL);
            g_signal_connect(state.associationDialog, "response",
                             G_CALLBACK(+[](GtkDialog*, gint response, gpointer context)
                                        {
                                            auto& viewer = *static_cast<ViewerApplication*>(context);
                                            viewer.fViewer.HandleEventCallback(
                                                [&]
                                                {
                                                    const auto choice = response == GTK_RESPONSE_YES
                                                                            ? FileAssociationChoice::Associate
                                                                        : response == GTK_RESPONSE_NO
                                                                            ? FileAssociationChoice::SuppressForCopy
                                                                        : response == SuppressForUserResponse
                                                                            ? FileAssociationChoice::SuppressForUser
                                                                            : FileAssociationChoice::AskNextTime;
                                                    viewer.CloseFileAssociationDialog();
                                                    if (!viewer.fViewer.fIsShuttingDown &&
                                                        viewer.fFileAssociations != nullptr)
                                                        viewer.fFileAssociations->Respond(choice);
                                                    return true;
                                                });
                                        }),
                             this);
            // Service GTK on the same UI thread without entering its modal loop. This keeps Wayland events,
            // repainting, and viewer shutdown responsive. The pump runs only while the prompt is visible.
            state.associationDialogPump = std::make_unique<LWS::HighPrecisionTimer>(
                fPlatform,
                [this]
                {
                    fViewer.HandleEventCallback(
                        [&]
                        {
                            while (gtk_events_pending())
                                gtk_main_iteration_do(FALSE);
                            return true;
                        });
                });
            constexpr uint32_t GtkEventIntervalMs = 16;
            state.associationDialogPump->SetRepeatInterval(GtkEventIntervalMs);
            state.associationDialogPump->SetDueTime(0);
            gtk_widget_show(state.associationDialog);
            gdk_display_flush(gdk_display_get_default());
            state.associationDialogPump->Enable(true);
        }
        else if (!notice.message.empty())
            fViewer.SetUserMessage(notice.message);
    }
}  // namespace OIV
