#include "PlatformFileDialog.h"

namespace OIV
{
    LWS::FileDialogResult PlatformFileDialog::Show(LWS::FileDialogType dialogType,
                                                   const LWS::ListFileDialogFilters& filters,
                                                   const LWS::file_dialog_string_type& title, LWS::Window& ownerWindow,
                                                   const LWS::file_dialog_string_type& defaultExtension,
                                                   uint32_t filterIndex, LWS::file_dialog_string_type defaultFileName,
                                                   LWS::file_dialog_string_type& outFilename)
    {
        return LWS::FileDialog::Show(dialogType, filters, title, ownerWindow, defaultExtension, filterIndex,
                                     std::move(defaultFileName), outFilename);
    }

    LWS::FileDialogResult PlatformFileDialog::Show(LWS::FileDialogType dialogType,
                                                   const LWS::ListFileDialogFilters& filters,
                                                   const LWS::file_dialog_string_type& title, LWS::Window& ownerWindow,
                                                   const LWS::file_dialog_string_type& defaultExtension,
                                                   uint32_t filterIndex, LWS::file_dialog_string_type defaultFileName,
                                                   LWS::ListFileDialogFileNames& outFilenames)
    {
        return LWS::FileDialog::Show(dialogType, filters, title, ownerWindow, defaultExtension, filterIndex,
                                     std::move(defaultFileName), outFilenames);
    }
}  // namespace OIV
