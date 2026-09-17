#pragma once

#include <LWS/FileDialog.hpp>

namespace OIV
{
    class PlatformFileDialog
    {
      public:

        static LWS::FileDialogResult Show(LWS::FileDialogType dialogType, const LWS::ListFileDialogFilters& filters,
                                          const LWS::file_dialog_string_type& title, LWS::Window& ownerWindow,
                                          const LWS::file_dialog_string_type& defaultExtension, uint32_t filterIndex,
                                          LWS::file_dialog_string_type defaultFileName,
                                          LWS::file_dialog_string_type& outFilename);

        static LWS::FileDialogResult Show(LWS::FileDialogType dialogType, const LWS::ListFileDialogFilters& filters,
                                          const LWS::file_dialog_string_type& title, LWS::Window& ownerWindow,
                                          const LWS::file_dialog_string_type& defaultExtension, uint32_t filterIndex,
                                          LWS::file_dialog_string_type defaultFileName,
                                          LWS::ListFileDialogFileNames& outFilenames);
    };
}  // namespace OIV
