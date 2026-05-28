#include <LLUtils/StringDefs.h>
#include <OIVAppCore/FileChangePolicy.h>

#include <filesystem>

namespace OIV
{
    FileChangeAction FileChangePolicy::Decide(const IFileWatcher::FileChangedEventArgs& eventArgs, bool hasActiveFolder,
                                              IFileWatcher::FolderID activeFolderID,
                                              IFileWatcher::FolderID configurationFolderID,
                                              const LLUtils::native_string_type& openedFile)
    {
        if (hasActiveFolder && eventArgs.folderID == activeFolderID)
        {
            const LLUtils::native_string_type changedFile =
                (std::filesystem::path(eventArgs.folder) / eventArgs.fileName).native();
            const LLUtils::native_string_type renamedFile =
                (std::filesystem::path(eventArgs.folder) / eventArgs.fileName2).native();

            switch (eventArgs.fileOp)
            {
                case IFileWatcher::FileChangedOp::Modified:
                    return openedFile == changedFile ? FileChangeAction::CurrentFileChanged : FileChangeAction::Ignore;
                case IFileWatcher::FileChangedOp::Rename:
                    return openedFile == renamedFile ? FileChangeAction::CurrentFileChanged : FileChangeAction::Ignore;
                case IFileWatcher::FileChangedOp::WatchedFolderRemoved:
                    return FileChangeAction::ClearWatchedFolder;
                case IFileWatcher::FileChangedOp::None:
                case IFileWatcher::FileChangedOp::Add:
                case IFileWatcher::FileChangedOp::Remove:
                    return FileChangeAction::Ignore;
            }
        }

        if (eventArgs.folderID == configurationFolderID)
            return eventArgs.fileName == LLUTILS_TEXT("Settings.json") ? FileChangeAction::ReloadSettings
                                                                       : FileChangeAction::Ignore;

        return FileChangeAction::Ignore;
    }
}  // namespace OIV
