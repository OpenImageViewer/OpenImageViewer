// Loaded only by the Linux subprocess checks. Inspect real GTK contents, then let its normal modal loop run.
#include <gtk/gtk.h>

#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace
{
    void FindHeaderButton(GtkWidget* widget, gpointer data)
    {
        auto& button = *static_cast<GtkWidget**>(data);
        if (GTK_IS_BUTTON(widget))
            button = widget;
        else if (GTK_IS_CONTAINER(widget))
            gtk_container_forall(GTK_CONTAINER(widget), FindHeaderButton, data);
    }

    void CollectDetails(GtkWidget* widget, gpointer data)
    {
        if (GTK_IS_TEXT_VIEW(widget))
        {
            GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
            GtkTextIter begin{}, end{};
            gtk_text_buffer_get_bounds(buffer, &begin, &end);
            char* text = gtk_text_buffer_get_text(buffer, &begin, &end, FALSE);
            static_cast<std::string*>(data)->append(text);
            g_free(text);
        }
        else if (GTK_IS_CONTAINER(widget))
        {
            gtk_container_foreach(GTK_CONTAINER(widget), CollectDetails, data);
        }
    }
}  // namespace

extern "C" gint gtk_dialog_run(GtkDialog* dialog)
{
    using RunDialog       = gint (*)(GtkDialog*);
    const auto run        = reinterpret_cast<RunDialog>(dlsym(RTLD_NEXT, "gtk_dialog_run"));
    const char* directory = std::getenv("OIV_TEST_DIALOG_DIR");
    if (run == nullptr || directory == nullptr)
        std::abort();

    std::string details;
    CollectDetails(GTK_WIDGET(dialog), &details);
    {
        const auto path = std::filesystem::path(directory) / (std::to_string(getpid()) + ".txt");
        std::ofstream output(path, std::ios::app);
        output << gtk_window_get_title(GTK_WINDOW(dialog)) << '\n' << details;
    }
    g_timeout_add(
        100,
        [](gpointer widget) -> gboolean
        {
            const char* control = std::getenv("OIV_TEST_DIALOG_CLOSE");
            GtkWidget* close{};
            if (control != nullptr && std::string_view(control) == "titlebar")
            {
                GtkWidget* header = gtk_window_get_titlebar(GTK_WINDOW(widget));
                if (!GTK_IS_HEADER_BAR(header) || !gtk_header_bar_get_show_close_button(GTK_HEADER_BAR(header)))
                    std::abort();
                FindHeaderButton(header, &close);
            }
            else
            {
                close = gtk_dialog_get_widget_for_response(GTK_DIALOG(widget), GTK_RESPONSE_CLOSE);
            }
            if (close == nullptr || !gtk_widget_is_sensitive(close) || !gtk_widget_get_visible(close))
                std::abort();
            gtk_button_clicked(GTK_BUTTON(close));
            return G_SOURCE_REMOVE;
        },
        dialog);
    const auto response = run(dialog);
    const char* control = std::getenv("OIV_TEST_DIALOG_CLOSE");
    const auto expected = control != nullptr && std::string_view(control) == "titlebar" ? GTK_RESPONSE_DELETE_EVENT
                                                                                        : GTK_RESPONSE_CLOSE;
    if (response != expected)
        std::abort();
    std::ofstream(std::filesystem::path(directory) / (std::to_string(getpid()) + ".closed")) << "closed";
    return response;
}
