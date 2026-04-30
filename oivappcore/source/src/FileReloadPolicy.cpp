#include <oivappcore/FileReloadPolicy.h>

namespace OIV
{
    void FileReloadPolicy::SetMode(FileReloadMode mode)
    {
        fMode = mode;
    }

    FileReloadMode FileReloadPolicy::GetMode() const
    {
        return fMode;
    }

    const std::wstring& FileReloadPolicy::GetPendingReloadFile() const
    {
        return fPendingReloadFile;
    }

    bool FileReloadPolicy::HasPendingReloadFor(const std::wstring& fileName) const
    {
        return !fPendingReloadFile.empty() && fPendingReloadFile == fileName;
    }

    ReloadAction FileReloadPolicy::OnCurrentFileChanged(const std::wstring& openedFile, bool appActive)
    {
        switch (fMode)
        {
            case FileReloadMode::AutoBackground:
                return ReloadAction::RequestNow;
            case FileReloadMode::AutoForeground:
                if (appActive)
                    return ReloadAction::RequestNow;
                fPendingReloadFile = openedFile;
                return ReloadAction::Defer;
            case FileReloadMode::Confirmation:
                fPendingReloadFile = openedFile;
                return appActive ? ReloadAction::AskUser : ReloadAction::Defer;
            case FileReloadMode::None:
                break;
        }

        return ReloadAction::None;
    }

    ReloadAction FileReloadPolicy::OnPendingReloadRequested(const std::wstring& requestedFile)
    {
        if (!HasPendingReloadFor(requestedFile))
            return ReloadAction::None;

        if (fMode == FileReloadMode::Confirmation)
            return ReloadAction::AskUser;

        fPendingReloadFile.clear();
        return fMode == FileReloadMode::None ? ReloadAction::None : ReloadAction::RequestNow;
    }

    ReloadAction FileReloadPolicy::ConfirmReload(bool accepted)
    {
        fPendingReloadFile.clear();
        return accepted ? ReloadAction::RequestNow : ReloadAction::None;
    }
}  // namespace OIV
