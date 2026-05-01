#pragma once

#include <string>

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
        const std::wstring& GetPendingReloadFile() const;
        bool HasPendingReloadFor(const std::wstring& fileName) const;
        ReloadAction OnCurrentFileChanged(const std::wstring& openedFile, bool appActive);
        ReloadAction OnPendingReloadRequested(const std::wstring& requestedFile);
        ReloadAction ConfirmReload(bool accepted);

      private:
        FileReloadMode fMode = FileReloadMode::Confirmation;
        std::wstring fPendingReloadFile;
    };
}  // namespace OIV
