#pragma once

#include <LLUtils/StringDefs.h>

namespace OIV
{
    enum class FileReloadMode
    {
        None,
        Confirmation,
        AutoForeground,
        AutoBackground
    };

    enum class ReloadAction
    {
        None,
        RequestNow,
        AskUser,
        Defer
    };

    class FileReloadPolicy
    {
      public:

        void SetMode(FileReloadMode mode);
        FileReloadMode GetMode() const;
        const LLUtils::native_string_type& GetPendingReloadFile() const;
        bool HasPendingReloadFor(const LLUtils::native_string_type& fileName) const;
        ReloadAction OnCurrentFileChanged(const LLUtils::native_string_type& openedFile, bool appActive);
        ReloadAction OnPendingReloadRequested(const LLUtils::native_string_type& requestedFile);
        ReloadAction ConfirmReload(bool accepted);

      private:

        FileReloadMode fMode = FileReloadMode::Confirmation;
        LLUtils::native_string_type fPendingReloadFile;
    };
}  // namespace OIV
