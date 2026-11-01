#include "Helpers/ShellCommandHandler.h"

#include <LLUtils/Exception.h>

#include <gio/gio.h>

#include <filesystem>

namespace OIV
{
    namespace
    {
        bool IsUnavailableError(const GError* error)
        {
            return error != nullptr && error->domain == G_DBUS_ERROR &&
                   (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN || error->code == G_DBUS_ERROR_NAME_HAS_NO_OWNER ||
                    error->code == G_DBUS_ERROR_UNKNOWN_INTERFACE || error->code == G_DBUS_ERROR_UNKNOWN_METHOD);
        }

        bool RevealInFileManager(const std::filesystem::path& path)
        {
            GError* error{};
            char* targetUri = g_filename_to_uri(path.c_str(), nullptr, &error);
            char* parentUri = g_filename_to_uri(path.parent_path().c_str(), nullptr, nullptr);
            if (targetUri == nullptr || parentUri == nullptr)
            {
                g_clear_error(&error);
                g_free(targetUri);
                g_free(parentUri);
                return false;
            }

            GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
            const char* uris[]{targetUri, nullptr};
            GVariant* reply = bus != nullptr
                                  ? g_dbus_connection_call_sync(bus, "org.freedesktop.FileManager1",
                                                                "/org/freedesktop/FileManager1",
                                                                "org.freedesktop.FileManager1", "ShowItems",
                                                                g_variant_new("(^ass)", uris, ""), nullptr,
                                                                G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &error)
                                  : nullptr;
            bool success    = reply != nullptr;
            if (!success && IsUnavailableError(error))
            {
                g_clear_error(&error);
                success = g_app_info_launch_default_for_uri(parentUri, nullptr, &error) != FALSE;
            }
            if (reply != nullptr)
                g_variant_unref(reply);
            if (bus != nullptr)
                g_object_unref(bus);
            g_clear_error(&error);
            g_free(targetUri);
            g_free(parentUri);
            return success;
        }
    }  // namespace

    LLUtils::native_string_type ShellCommandHandler::Execute(const CommandManager::CommandRequest& request,
                                                             const LLUtils::native_string_type& openedFileName,
                                                             [[maybe_unused]] const ViewerImage* openedImage)
    {
        const std::string command          = request.args.GetArgValue("cmd");
        LLUtils::native_string_type result = request.displayName;
        if (command == "containingFolder" && (openedFileName.empty() || !RevealInFileManager(openedFileName)))
        {
            result = "Unable to reveal the file";
        }
        return result;
    }
}  // namespace OIV
