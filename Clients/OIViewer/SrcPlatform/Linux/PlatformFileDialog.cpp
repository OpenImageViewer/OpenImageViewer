#include "PlatformFileDialog.h"

#include <gtk/gtk.h>

#include <filesystem>
#include <string_view>
#include <utility>

namespace
{
    struct DialogSelection
    {
        LWS::FileDialogResult result{LWS::FileDialogResult::UnknownError};
        LWS::ListFileDialogFileNames fileNames;
    };

    GtkFileChooserAction ToGtkAction(LWS::FileDialogType dialogType)
    {
        switch (dialogType)
        {
            case LWS::FileDialogType::OpenFile:
                return GTK_FILE_CHOOSER_ACTION_OPEN;
            case LWS::FileDialogType::SaveFile:
                return GTK_FILE_CHOOSER_ACTION_SAVE;
            case LWS::FileDialogType::Unspecified:
                return GTK_FILE_CHOOSER_ACTION_OPEN;
        }
        return GTK_FILE_CHOOSER_ACTION_OPEN;
    }

    std::string_view DefaultExtensionFromPattern(const LWS::file_dialog_string_type& pattern)
    {
        constexpr std::string_view wildcardPrefix = "*.";
        if (pattern.starts_with(wildcardPrefix) && pattern != "*.*")
            return std::string_view(pattern).substr(wildcardPrefix.size());
        return {};
    }

    void AddFilters(GtkFileChooser* chooser, const LWS::ListFileDialogFilters& filters, uint32_t filterIndex)
    {
        GtkFileFilter* selectedFilter{};
        for (size_t index = 0; index < filters.size(); ++index)
        {
            const auto& filter       = filters[index];
            GtkFileFilter* gtkFilter = gtk_file_filter_new();
            gtk_file_filter_set_name(gtkFilter, filter.description.c_str());
            for (const auto& extension : filter.extensions)
                gtk_file_filter_add_pattern(gtkFilter, extension == "*.*" ? "*" : extension.c_str());
            gtk_file_chooser_add_filter(chooser, gtkFilter);
            if (filterIndex == index + 1)
                selectedFilter = gtkFilter;
        }
        if (selectedFilter != nullptr)
            gtk_file_chooser_set_filter(chooser, selectedFilter);
    }

    void SetDefaultFileName(GtkFileChooser* chooser, LWS::FileDialogType dialogType,
                            const LWS::file_dialog_string_type& defaultFileName)
    {
        if (defaultFileName.empty())
            return;

        const std::filesystem::path path(defaultFileName);
        if (dialogType == LWS::FileDialogType::SaveFile)
        {
            if (path.has_parent_path())
                gtk_file_chooser_set_current_folder(chooser, path.parent_path().c_str());
            gtk_file_chooser_set_current_name(chooser, path.filename().c_str());
        }
        else
        {
            gtk_file_chooser_set_filename(chooser, path.c_str());
        }
    }

    DialogSelection ShowGtkDialog(LWS::FileDialogType dialogType, const LWS::ListFileDialogFilters& filters,
                                  const LWS::file_dialog_string_type& title,
                                  const LWS::file_dialog_string_type& defaultExtension, uint32_t filterIndex,
                                  const LWS::file_dialog_string_type& defaultFileName, bool allowMultiple)
    {
        DialogSelection selection;
        if (dialogType == LWS::FileDialogType::Unspecified || gtk_init_check(nullptr, nullptr) == FALSE)
            return selection;

        const GtkFileChooserAction action = ToGtkAction(dialogType);
        const char* acceptLabel           = dialogType == LWS::FileDialogType::SaveFile ? "_Save" : "_Open";
        GtkFileChooserNative* dialog      = gtk_file_chooser_native_new(title.c_str(), nullptr, action, acceptLabel,
                                                                        "_Cancel");
        if (dialog == nullptr)
            return selection;

        GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
        gtk_file_chooser_set_select_multiple(chooser, allowMultiple);
        if (dialogType == LWS::FileDialogType::SaveFile)
            gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
        AddFilters(chooser, filters, filterIndex);
        SetDefaultFileName(chooser, dialogType, defaultFileName);

        const int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
        if (response == GTK_RESPONSE_ACCEPT)
        {
            GSList* selectedFiles = gtk_file_chooser_get_filenames(chooser);
            for (GSList* item = selectedFiles; item != nullptr; item = item->next)
            {
                auto* fileName = static_cast<char*>(item->data);
                std::filesystem::path path(fileName);
                if (dialogType == LWS::FileDialogType::SaveFile && !path.has_extension())
                {
                    const std::string_view extension = DefaultExtensionFromPattern(defaultExtension);
                    if (!extension.empty())
                        path.replace_extension(extension);
                }
                selection.fileNames.push_back(path.native());
                g_free(fileName);
            }
            g_slist_free(selectedFiles);
            selection.result = selection.fileNames.empty() ? LWS::FileDialogResult::UnknownError
                                                           : LWS::FileDialogResult::Success;
        }
        else
        {
            selection.result = LWS::FileDialogResult::UserCanceled;
        }

        g_object_unref(dialog);
        // LWS runs the application's event loop, so GTK will not flush the dialog's unmap/destroy requests for us.
        gdk_display_flush(gdk_display_get_default());
        return selection;
    }
}  // namespace

namespace OIV
{
    LWS::FileDialogResult PlatformFileDialog::Show(LWS::FileDialogType dialogType,
                                                   const LWS::ListFileDialogFilters& filters,
                                                   const LWS::file_dialog_string_type& title,
                                                   [[maybe_unused]] LWS::Window& ownerWindow,
                                                   const LWS::file_dialog_string_type& defaultExtension,
                                                   uint32_t filterIndex, LWS::file_dialog_string_type defaultFileName,
                                                   LWS::file_dialog_string_type& outFilename)
    {
        DialogSelection selection = ShowGtkDialog(dialogType, filters, title, defaultExtension, filterIndex,
                                                  defaultFileName, false);
        if (selection.result == LWS::FileDialogResult::Success)
            outFilename = std::move(selection.fileNames.front());
        return selection.result;
    }

    LWS::FileDialogResult PlatformFileDialog::Show(LWS::FileDialogType dialogType,
                                                   const LWS::ListFileDialogFilters& filters,
                                                   const LWS::file_dialog_string_type& title,
                                                   [[maybe_unused]] LWS::Window& ownerWindow,
                                                   const LWS::file_dialog_string_type& defaultExtension,
                                                   uint32_t filterIndex, LWS::file_dialog_string_type defaultFileName,
                                                   LWS::ListFileDialogFileNames& outFilenames)
    {
        DialogSelection selection = ShowGtkDialog(dialogType, filters, title, defaultExtension, filterIndex,
                                                  defaultFileName, true);
        if (selection.result == LWS::FileDialogResult::Success)
            outFilenames = std::move(selection.fileNames);
        return selection.result;
    }
}  // namespace OIV
