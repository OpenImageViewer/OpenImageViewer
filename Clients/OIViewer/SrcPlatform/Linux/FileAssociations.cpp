#include "FileAssociations.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

namespace OIV::Linux
{
    namespace
    {
        constexpr char DesktopId[] = "org.openimageviewer.OIViewer.desktop";
        using KeyFile              = std::unique_ptr<GKeyFile, decltype(&g_key_file_unref)>;
        using AppInfo              = std::unique_ptr<GAppInfo, decltype(&g_object_unref)>;
        using String               = std::unique_ptr<char, decltype(&g_free)>;
        using Kind                 = FileAssociationNotice::Kind;

        FileAssociationNotice Failure(GError* error, const char* operation)
        {
            std::string message = operation;
            if (error != nullptr)
            {
                message += ": " + std::string(error->message);
                g_error_free(error);
            }
            return {Kind::ActionFailed, std::move(message)};
        }

        std::set<std::string> SupportedMimeTypes(const std::set<std::string>& extensions)
        {
            std::set<std::string> types;
            for (const auto& extension : extensions)
            {
                const String type(g_content_type_guess(("image." + extension).c_str(), nullptr, 0, nullptr), &g_free);
                if (type != nullptr && !g_content_type_is_unknown(type.get()))
                    types.emplace(type.get());
            }
            return types;
        }

        // Desktop Exec uses its own quoting rules, not shell quoting. Percent signs in the executable must
        // also be escaped so they cannot become desktop field codes. GKeyFile supplies the outer file escaping.
        // GIO validates the first token before expanding field codes. Using env keeps percent signs in the
        // executable path in an argument, where GIO expands them correctly.
        std::string DesktopCommand(const std::string& executable)
        {
            std::string command = "/usr/bin/env \"";
            for (char character : executable)
            {
                if (character == '%')
                    command += '%';
                else if (character == '\\' || character == '"' || character == '$' || character == '`')
                    command += '\\';
                command += character;
            }
            return command + "\" %f";
        }
        bool UsesExecutable(GAppInfo* application, const std::string& executable)
        {
            bool matches = false;
            if (application != nullptr)
            {
                const bool isViewer = G_IS_DESKTOP_APP_INFO(application) &&
                                      g_strcmp0(g_app_info_get_id(application), DesktopId) == 0;
                const String command(isViewer
                                         ? g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(application), "TryExec")
                                         : g_strdup(g_app_info_get_executable(application)),
                                     &g_free);
                const String resolved(command ? g_find_program_in_path(command.get()) : nullptr, &g_free);
                std::error_code error;
                matches = resolved && std::filesystem::equivalent(resolved.get(), executable, error) &&
                          (!isViewer || g_strcmp0(g_app_info_get_commandline(application),
                                                  DesktopCommand(command.get()).c_str()) == 0);
            }
            return matches;
        }
    }  // namespace

    FileAssociationNotice ManageFileAssociations(const std::filesystem::path& configDirectory,
                                                 const std::set<std::string>& extensions, const std::string& executable,
                                                 FileAssociationAction action, std::stop_token stop)
    {
        if (stop.stop_requested())
            return {Kind::Completed, {}};
        auto types = SupportedMimeTypes(extensions);
        if (action == FileAssociationAction::Check)
        {
            const bool needsAssociation = std::ranges::any_of(
                types,
                [&](const auto& type)
                {
                    const AppInfo current(g_app_info_get_default_for_type(type.c_str(), FALSE), &g_object_unref);
                    return !UsesExecutable(current.get(), executable);
                });
            if (!needsAssociation || stop.stop_requested())
                return {Kind::Completed, {}};
            return {Kind::Prompt, {}};
        }

        const auto statePath   = configDirectory / "associations.ini";
        const auto desktopPath = std::filesystem::path(g_get_user_data_dir()) / "applications" / DesktopId;
        std::error_code filesystemError;
        std::filesystem::create_directories(configDirectory, filesystemError);
        if (filesystemError)
            return {Kind::ActionFailed, "Could not create association state directory: " + filesystemError.message()};
        const KeyFile state(g_key_file_new(), &g_key_file_unref);
        GError* error{};
        if (!g_key_file_load_from_file(state.get(), statePath.c_str(), G_KEY_FILE_NONE, &error) &&
            !g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            return Failure(error, "Could not read file association state");
        g_clear_error(&error);
        if (action == FileAssociationAction::Remove && !std::filesystem::exists(desktopPath, filesystemError))
        {
            if (filesystemError)
                return {Kind::ActionFailed, "Could not inspect the desktop application: " + filesystemError.message()};
            return {Kind::Completed, "No OIViewer file associations are registered."};
        }

        if (action == FileAssociationAction::Add)
        {
            if (types.empty())
                return {Kind::ActionFailed,
                        "The desktop MIME database recognizes none of the supported image extensions."};
            std::filesystem::create_directories(desktopPath.parent_path(), filesystemError);
            if (filesystemError)
                return {Kind::ActionFailed,
                        "Could not create the desktop application directory: " + filesystemError.message()};
            const KeyFile desktop(g_key_file_new(), &g_key_file_unref);
            g_key_file_set_string(desktop.get(), "Desktop Entry", "Type", "Application");
            g_key_file_set_string(desktop.get(), "Desktop Entry", "Name", "Open Image Viewer");
            g_key_file_set_string(desktop.get(), "Desktop Entry", "Exec", DesktopCommand(executable).c_str());
            g_key_file_set_string(desktop.get(), "Desktop Entry", "TryExec", executable.c_str());
            g_key_file_set_string(desktop.get(), "Desktop Entry", "Icon", "image-x-generic");
            g_key_file_set_boolean(desktop.get(), "Desktop Entry", "Terminal", FALSE);
            std::vector<const char*> mimeTypes;
            for (const auto& type : types)
                mimeTypes.push_back(type.c_str());
            g_key_file_set_string_list(desktop.get(), "Desktop Entry", "MimeType", mimeTypes.data(), mimeTypes.size());
            if (!g_key_file_save_to_file(desktop.get(), desktopPath.c_str(), &error))
                return Failure(error, "Could not register the desktop application");
        }
        const AppInfo viewer(G_APP_INFO(g_desktop_app_info_new_from_filename(desktopPath.c_str())), &g_object_unref);
        if (!viewer)
            return {Kind::ActionFailed, "Could not load the registered OIViewer desktop application."};
        if (!UsesExecutable(viewer.get(), executable))
            return {Kind::ActionFailed, "File associations belong to another copy of OIViewer."};

        // Keep the original default through repeated Add operations and retain types from older codec sets.
        // Save each backup before changing its default so an interrupted operation can still be undone.
        if (action == FileAssociationAction::Remove)
        {
            gsize count{};
            char** savedTypes = g_key_file_get_keys(state.get(), "Previous", &count, nullptr);
            for (gsize index = 0; index < count; ++index)
                types.emplace(savedTypes[index]);
            g_strfreev(savedTypes);
        }
        for (const auto& type : types)
        {
            if (stop.stop_requested())
                return {Kind::Completed, {}};
            const AppInfo current(g_app_info_get_default_for_type(type.c_str(), FALSE), &g_object_unref);
            const char* currentId = current ? g_app_info_get_id(current.get()) : nullptr;
            const bool isViewer   = g_strcmp0(currentId, DesktopId) == 0;
            if (action == FileAssociationAction::Add)
            {
                if (!isViewer)
                {
                    g_key_file_set_string(state.get(), "Previous", type.c_str(), currentId != nullptr ? currentId : "");
                    if (!g_key_file_save_to_file(state.get(), statePath.c_str(), &error))
                        return Failure(error, "Could not save the previous default application");
                }
                if (!g_app_info_set_as_default_for_type(viewer.get(), type.c_str(), &error))
                    return Failure(error, "Could not set OIViewer as the default application");
            }
            else
            {
                if (isViewer)
                {
                    const String previousId(g_key_file_get_string(state.get(), "Previous", type.c_str(), nullptr),
                                            &g_free);
                    const AppInfo previous(previousId && *previousId
                                               ? G_APP_INFO(g_desktop_app_info_new(previousId.get()))
                                               : nullptr,
                                           &g_object_unref);
                    // Remove only our association. Never reset a MIME type's complete association list.
                    if (!g_app_info_remove_supports_type(viewer.get(), type.c_str(), &error))
                        return Failure(error, "Could not remove OIViewer's file association");
                    if (previous && !g_app_info_set_as_default_for_type(previous.get(), type.c_str(), &error))
                        return Failure(error, "Could not restore the previous default application");
                }
                g_key_file_remove_key(state.get(), "Previous", type.c_str(), nullptr);
            }
        }
        if (!g_key_file_save_to_file(state.get(), statePath.c_str(), &error))
            return Failure(error, "Could not save file association state");
        if (action == FileAssociationAction::Remove && g_unlink(desktopPath.c_str()) != 0)
            return {Kind::ActionFailed, "Could not remove the OIViewer desktop application."};
        return {Kind::Completed, action == FileAssociationAction::Add
                                     ? "OIViewer is now the default for supported desktop image types."
                                     : "OIViewer associations removed. Available saved defaults restored."};
    }
}  // namespace OIV::Linux
