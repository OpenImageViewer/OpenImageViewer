// Test-only interposition observes the actual GTK dialog and the separate LWS Wayland connection.
#include <gtk/gtk.h>
#include <wayland-client.h>
#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <poll.h>

namespace
{
    wl_display* viewerDisplay{};
    wl_proxy* viewerToplevel{};
    void* viewerContext{};
    void (*closeViewer)(void*, wl_proxy*){};
    unsigned viewerDispatches{};
    GtkWidget* prompt{};
    unsigned promptDispatches{};
    std::filesystem::path Marker(const char* suffix)
    {
        return std::filesystem::path(std::getenv("OIV_TEST_ASSOCIATION_DIR")) / (std::to_string(getpid()) + suffix);
    }
    gboolean CheckPrompt(gpointer)
    {
        if (prompt == nullptr)
            return G_SOURCE_REMOVE;
        if (viewerDispatches >= promptDispatches + 3)
            std::ofstream(Marker(".responsive")) << "responsive";
        if (std::filesystem::exists(Marker(".cancel")))
            gtk_dialog_response(GTK_DIALOG(prompt), GTK_RESPONSE_CANCEL);
        return prompt == nullptr ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
    }
}  // namespace
// xdg_toplevel is created by LWS before GTK. Reuse its native close listener to exercise real viewer teardown.
extern "C" int wl_proxy_add_listener(wl_proxy* proxy, void (**implementation)(void), void* context)
{
    static const auto native = reinterpret_cast<decltype(&wl_proxy_add_listener)>(
        dlsym(RTLD_NEXT, "wl_proxy_add_listener"));
    if (viewerToplevel == nullptr && std::string_view(wl_proxy_get_class(proxy)) == "xdg_toplevel")
    {
        viewerToplevel = proxy;
        viewerContext  = context;
        closeViewer    = reinterpret_cast<decltype(closeViewer)>(implementation[1]);
    }
    return native(proxy, implementation, context);
}
extern "C" int wl_display_dispatch_pending(wl_display* display)
{
    static const auto native = reinterpret_cast<decltype(&wl_display_dispatch_pending)>(
        dlsym(RTLD_NEXT, "wl_display_dispatch_pending"));
    if (viewerDisplay == nullptr)
        viewerDisplay = display;
    if (display == viewerDisplay)
    {
        ++viewerDispatches;
        if (closeViewer != nullptr && std::filesystem::exists(Marker(".close-viewer")))
        {
            std::filesystem::remove(Marker(".close-viewer"));
            closeViewer(viewerContext, viewerToplevel);
        }
    }
    return native(display);
}
extern "C" void gtk_widget_show(GtkWidget* widget)
{
    static const auto native = reinterpret_cast<decltype(&gtk_widget_show)>(dlsym(RTLD_NEXT, "gtk_widget_show"));
    native(widget);
    if (GTK_IS_MESSAGE_DIALOG(widget) && prompt == nullptr)
    {
        prompt           = widget;
        promptDispatches = viewerDispatches;
        g_object_weak_ref(
            G_OBJECT(widget),
            +[](gpointer, GObject*)
            {
                prompt = nullptr;
                std::ofstream(Marker(".closed")) << "closed";
            },
            nullptr);
        std::ofstream(Marker(".prompt")) << "prompt";
        g_timeout_add(50, CheckPrompt, nullptr);
    }
}

// Keep the test control channel observable after the viewer becomes idle. Production polling is unchanged.
extern "C" int poll(pollfd* descriptors, nfds_t count, int timeout)
{
    static const auto native = reinterpret_cast<decltype(&poll)>(dlsym(RTLD_NEXT, "poll"));
    return native(descriptors, count, timeout < 0 || timeout > 50 ? 50 : timeout);
}
