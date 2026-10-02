#include "ExceptionHandler.h"
#include <gtk/gtk.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <tuple>
#include <unistd.h>

namespace OIV
{
    namespace
    {
        constexpr char DialogArgument[] = "--oiv-internal-exception-dialog";
        constexpr char Fallback[]       = "OIViewer encountered an unhandled exception. Diagnostics are unavailable.\n";
    }  // namespace

    void detail::PresentExceptionReport(std::string_view message) noexcept
    {
        if (message.empty())
            message = Fallback;
        std::ignore = ::write(STDERR_FILENO, message.data(), message.size());

        // A fatal error may come from a worker or from GTK itself. Re-exec into a fresh process so the
        // dialog owns its UI thread and inherits no toolkit locks. No shell or optional dialog utility is used.
        char* const arguments[] = {const_cast<char*>("/proc/self/exe"), const_cast<char*>(DialogArgument),
                                   const_cast<char*>(message.data()), nullptr};
        pid_t child{};
        if (posix_spawn(&child, "/proc/self/exe", nullptr, nullptr, arguments, environ) == 0)
        {
            while (waitpid(child, nullptr, 0) == -1 && errno == EINTR)
            {
            }
        }
    }

    std::optional<int> RunExceptionDialog(int argc, char* argv[])
    {
        if (argc != 3 || std::strcmp(argv[1], DialogArgument) != 0)
            return std::nullopt;
        if (!gtk_init_check(nullptr, nullptr))
            return EXIT_FAILURE;  // The reporting parent has already written the diagnostic to stderr.

        GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                   "%s", "OIViewer encountered an unhandled exception and must close.");
        gtk_window_set_title(GTK_WINDOW(dialog), "OIViewer - Unhandled exception");
        gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);

        // GtkMessageDialog's plain title bar omits window controls. This standalone dialog needs its own
        // draggable header and close button rather than relying on compositor-provided decorations.
        GtkWidget* header = gtk_header_bar_new();
        gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OIViewer - Unhandled exception");
        gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
        gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(header), ":close");
        gtk_window_set_titlebar(GTK_WINDOW(dialog), header);

        GtkWidget* close = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
        gtk_widget_set_halign(gtk_widget_get_parent(close), GTK_ALIGN_END);

        // Reports can contain several nested stacks. Keep the message box usable on small displays.
        GtkWidget* details = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(details), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(details), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(details), GTK_WRAP_WORD_CHAR);
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(details)), argv[2], -1);
        GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scroll, 640, 280);
        gtk_container_add(GTK_CONTAINER(scroll), details);
        gtk_box_pack_start(GTK_BOX(gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog))), scroll, TRUE, TRUE,
                           0);
        gtk_widget_show_all(dialog);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        // The helper exits without another GTK iteration; send the unmap/destroy requests before disconnecting.
        gdk_display_flush(gdk_display_get_default());
        return EXIT_SUCCESS;
    }

    ExceptionRegistration::ExceptionRegistration() noexcept
        : fPreviousTerminate(std::set_terminate(detail::TerminateAfterException))
    {
    }
    ExceptionRegistration::~ExceptionRegistration()
    {
        std::set_terminate(fPreviousTerminate);
    }
}  // namespace OIV
