#include <LLUtils/StringDefs.h>
#include <OIVAppCore/FileReloadPolicy.h>

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

    const LLUtils::native_string_type& FileReloadPolicy::GetPendingReloadFile() const
    {
        return fPendingReloadFile;
    }

    bool FileReloadPolicy::HasPendingReloadFor(const LLUtils::native_string_type& fileName) const
    {
        return !fPendingReloadFile.empty() && fPendingReloadFile == fileName;
    }

    ReloadAction FileReloadPolicy::OnCurrentFileChanged(const LLUtils::native_string_type& openedFile, bool appActive)
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
                // Consume the request before a modal prompt can reactivate its owner and ask again.
                if (appActive)
                {
                    fPendingReloadFile.clear();
                    return ReloadAction::AskUser;
                }
                fPendingReloadFile = openedFile;
                return ReloadAction::Defer;
            case FileReloadMode::None:
                break;
        }

        return ReloadAction::None;
    }

    ReloadAction FileReloadPolicy::OnPendingReloadRequested(const LLUtils::native_string_type& requestedFile)
    {
        if (!HasPendingReloadFor(requestedFile))
            return ReloadAction::None;

        // Native activation may reenter before the prompt returns; this request is already being handled.
        fPendingReloadFile.clear();
        if (fMode == FileReloadMode::Confirmation)
            return ReloadAction::AskUser;

        return fMode == FileReloadMode::None ? ReloadAction::None : ReloadAction::RequestNow;
    }

    ReloadAction FileReloadPolicy::ConfirmReload(bool accepted)
    {
        // A newer change may have been deferred while the user answered the previous prompt.
        return accepted ? ReloadAction::RequestNow : ReloadAction::None;
    }
}  // namespace OIV
